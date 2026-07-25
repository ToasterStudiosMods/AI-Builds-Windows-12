/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * virtio_net.c — virtio-net over legacy virtio-pci
 *
 * A virtqueue is three contiguous structures the driver and device share:
 *
 *   descriptor table   what each buffer is: address, length, flags
 *   available ring     driver writes here to offer buffers to the device
 *   used ring          device writes here to hand buffers back
 *
 * The driver publishes a buffer by filling a descriptor, putting its index in
 * the available ring, bumping that ring's index, and kicking the queue's notify
 * port. Completion is the mirror image: the device bumps the used ring's index
 * and the driver walks forward from whatever it saw last. Queue 0 receives,
 * queue 1 transmits.
 * ==========================================================================*/

#include "virtio_net.h"
#include "pci.h"
#include "mem.h"
#include "io.h"
#include "serial.h"
#include "string.h"

/* --- legacy virtio-pci register block, offsets from the I/O BAR --- */
#define VP_HOST_FEATURES   0x00
#define VP_GUEST_FEATURES  0x04
#define VP_QUEUE_PFN       0x08
#define VP_QUEUE_NUM       0x0C
#define VP_QUEUE_SEL       0x0E
#define VP_QUEUE_NOTIFY    0x10
#define VP_STATUS          0x12
#define VP_ISR             0x13
#define VP_CONFIG          0x14      /* device-specific config starts here */

#define VS_ACKNOWLEDGE     1
#define VS_DRIVER          2
#define VS_DRIVER_OK       4
#define VS_FAILED          0x80

/* virtio-net feature bits */
#define VNET_F_MAC         (1u << 5)
#define VNET_F_STATUS      (1u << 16)
#define VNET_F_MRG_RXBUF   (1u << 15)

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

/* Header preceding every frame on both queues. Ten bytes as long as mergeable
 * receive buffers are NOT negotiated, which is exactly why we do not ask for
 * that feature — it would silently make this twelve and shift every frame. */
#define HDR_LEN 10
#define BUF_SZ  2048
#define NBUF    32          /* buffers we post per queue */

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

struct vq {
    uint16_t size;              /* descriptors the device gave us       */
    uint16_t nbuf;              /* buffers we actually use              */
    uint16_t index;
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    uint16_t last_used;         /* how far we consumed the used ring    */
    uint16_t next_buf;          /* round-robin hint over our buffers    */
    uint8_t *bufs;              /* nbuf * BUF_SZ, contiguous            */
    uint8_t *hdrs;              /* nbuf * HDR_LEN, transmit only        */
    uint8_t  posted[NBUF];      /* buffer currently owned by the device */
};

static struct virtio_net_state st;
static struct vq rxq, txq;
/* Received frames are copied out before the buffer goes back to the device, so
 * the caller cannot be reading memory the device has started refilling. */
static uint8_t rx_scratch[BUF_SZ];

static inline void wr8(uint16_t off, uint8_t v)   { outb(st.iobase + off, v); }
static inline uint8_t  rd8(uint16_t off)          { return inb(st.iobase + off); }
static inline void wr16(uint16_t off, uint16_t v) { outw(st.iobase + off, v); }
static inline uint16_t rd16(uint16_t off)         { return inw(st.iobase + off); }
static inline void wr32(uint16_t off, uint32_t v) { outl(st.iobase + off, v); }
static inline uint32_t rd32(uint16_t off)         { return inl(st.iobase + off); }

/* The device writes used->idx behind our back, so every read of it has to
 * actually happen — no caching it in a register across a loop. */
static inline uint16_t used_idx(struct vq *q)
{
    return *(volatile uint16_t *)&q->used->idx;
}

/* Lay out one virtqueue in a single physically contiguous allocation. The used
 * ring must start on a page boundary, which is what the padding is for. */
static int vq_init(struct vq *q, uint16_t index, int need_hdrs)
{
    memset(q, 0, sizeof(*q));
    q->index = index;

    wr16(VP_QUEUE_SEL, index);
    q->size = rd16(VP_QUEUE_NUM);
    if (q->size == 0) {
        serial_write("[virtio] queue absent\n");
        return 0;
    }

    /* Transmit chains two descriptors per frame, so never claim more buffers
     * than half the table. */
    uint16_t cap = need_hdrs ? (uint16_t)(q->size / 2) : q->size;
    q->nbuf = (cap < NBUF) ? cap : NBUF;
    if (q->nbuf == 0) return 0;

    uint32_t desc_bytes  = 16u * q->size;
    uint32_t avail_bytes = 4u + 2u * q->size + 2u;
    uint32_t used_off    = (desc_bytes + avail_bytes + 0xFFFu) & ~0xFFFu;
    uint32_t used_bytes  = 4u + 8u * q->size + 2u;

    uint8_t *mem = (uint8_t *)phys_alloc(used_off + used_bytes);
    if (!mem) return 0;
    memset(mem, 0, used_off + used_bytes);

    q->desc  = (struct vring_desc  *)mem;
    q->avail = (struct vring_avail *)(mem + desc_bytes);
    q->used  = (struct vring_used  *)(mem + used_off);

    q->bufs = (uint8_t *)phys_alloc((uint64_t)q->nbuf * BUF_SZ);
    if (!q->bufs) return 0;
    if (need_hdrs) {
        q->hdrs = (uint8_t *)phys_alloc((uint64_t)q->nbuf * HDR_LEN);
        if (!q->hdrs) return 0;
        memset(q->hdrs, 0, (uint64_t)q->nbuf * HDR_LEN);
    }

    /* The device is told where the queue lives as a page frame number, which is
     * why the identity map matters: our pointer is already the physical
     * address. */
    wr32(VP_QUEUE_PFN, (uint32_t)((uint64_t)(uintptr_t)mem >> 12));

    serial_write("[virtio] queue ");    serial_write_u64(index);
    serial_write(" size ");             serial_write_u64(q->size);
    serial_write(" using ");            serial_write_u64(q->nbuf);
    serial_write(" buffers\n");
    return 1;
}

/* Offer a descriptor chain, starting at `head`, to the device. */
static void vq_publish(struct vq *q, uint16_t head)
{
    q->avail->ring[q->avail->idx % q->size] = head;
    __asm__ volatile ("" ::: "memory");      /* ring entry before the index */
    q->avail->idx++;
    __asm__ volatile ("" ::: "memory");      /* index before the kick       */
    wr16(VP_QUEUE_NOTIFY, q->index);
}

/* Hand a receive buffer to the device, marked writable so it can fill it. */
static void rx_post(uint16_t slot)
{
    struct vring_desc *d = &rxq.desc[slot];
    d->addr  = (uint64_t)(uintptr_t)(rxq.bufs + (uint64_t)slot * BUF_SZ);
    d->len   = BUF_SZ;
    d->flags = VRING_DESC_F_WRITE;
    d->next  = 0;
    rxq.posted[slot] = 1;
    vq_publish(&rxq, slot);
}

int virtio_net_init(void)
{
    memset(&st, 0, sizeof(st));

    /* Find a virtio network device: Red Hat vendor, device id 0x1000. */
    int idx = -1;
    for (int i = 0; i < pci_count(); i++) {
        const struct pci_dev *d = pci_get(i);
        if (d->vendor == 0x1AF4 && d->device == 0x1000) { idx = i; break; }
    }
    if (idx < 0) return 0;               /* not fitted; caller may try e1000 */

    const struct pci_dev *d = pci_get(idx);
    /* Legacy virtio-pci exposes its registers through an I/O BAR: bit 0 set,
     * with the port base in the upper bits. A modern virtio-1.0-only device
     * would present memory here instead, and we decline it rather than
     * misread the window. */
    if (!(d->bar[0] & 1)) {
        serial_write("[virtio] BAR0 is not an I/O window (not legacy-capable)\n");
        return 0;
    }
    st.iobase = (uint16_t)(d->bar[0] & ~0x3u);
    pci_enable(d);              /* bus mastering: the queues live in our RAM */
    serial_write("[virtio] iobase "); serial_write_hex(st.iobase); serial_write("\n");

    /* Reset, then walk the handshake: acknowledge, claim, negotiate. */
    wr8(VP_STATUS, 0);
    (void)rd8(VP_STATUS);
    wr8(VP_STATUS, VS_ACKNOWLEDGE);
    wr8(VP_STATUS, VS_ACKNOWLEDGE | VS_DRIVER);

    uint32_t host = rd32(VP_HOST_FEATURES);
    /* Take the MAC and link-status bits and nothing else. */
    uint32_t want = host & (VNET_F_MAC | VNET_F_STATUS);
    wr32(VP_GUEST_FEATURES, want);
    st.features = want;

    serial_write("[virtio] features host "); serial_write_hex(host);
    serial_write(" using ");                 serial_write_hex(want);
    serial_write("\n");

    if (want & VNET_F_MAC) {
        for (int i = 0; i < 6; i++) st.mac[i] = rd8((uint16_t)(VP_CONFIG + i));
    } else {
        /* No MAC from the device: use a locally administered one. */
        st.mac[0] = 0x02; st.mac[1] = 0x00; st.mac[2] = 0x00;
        st.mac[3] = 0xAE; st.mac[4] = 0x11; st.mac[5] = 0x01;
    }
    serial_write("[virtio] mac ");
    for (int i = 0; i < 6; i++) {
        serial_write_hex8(st.mac[i]);
        serial_write(i < 5 ? ":" : "\n");
    }

    if (!vq_init(&rxq, 0, 0) || !vq_init(&txq, 1, 1)) {
        wr8(VP_STATUS, VS_FAILED);
        return 0;
    }
    st.rxq_size = rxq.size;
    st.txq_size = txq.size;

    /* Live now: the device may start using the queues. */
    wr8(VP_STATUS, VS_ACKNOWLEDGE | VS_DRIVER | VS_DRIVER_OK);

    /* Give the receive queue somewhere to put incoming frames. */
    for (uint16_t i = 0; i < rxq.nbuf; i++) rx_post(i);

    st.present = 1;
    /* With the status feature the device reports link state in its config space
     * just past the MAC; without it, assume the link is up. */
    if (want & VNET_F_STATUS) {
        uint16_t s = (uint16_t)rd8(VP_CONFIG + 6) |
                     ((uint16_t)rd8(VP_CONFIG + 7) << 8);
        st.link_up = (s & 1) ? 1 : 0;
    } else {
        st.link_up = 1;
    }
    serial_write("[virtio] ready, link ");
    serial_write(st.link_up ? "up\n" : "down\n");
    return 1;
}

int virtio_net_send(const void *frame, uint16_t len)
{
    if (!st.present || len == 0 || len > BUF_SZ) return 0;

    /* Find one of our transmit buffers the device is not holding. */
    uint16_t slot = 0xFFFF;
    for (uint16_t i = 0; i < txq.nbuf; i++) {
        uint16_t c = (uint16_t)((txq.next_buf + i) % txq.nbuf);
        if (!txq.posted[c]) { slot = c; break; }
    }
    if (slot == 0xFFFF) return 0;                 /* all in flight */
    txq.next_buf = (uint16_t)((slot + 1) % txq.nbuf);

    uint8_t *hdr = txq.hdrs + (uint64_t)slot * HDR_LEN;
    uint8_t *buf = txq.bufs + (uint64_t)slot * BUF_SZ;
    memset(hdr, 0, HDR_LEN);                      /* no checksum or GSO offload */
    memcpy(buf, frame, len);

    /* Two chained descriptors: the header, then the frame. Putting both in one
     * buffer is only legal with VIRTIO_F_ANY_LAYOUT negotiated — chaining is
     * always legal, so do that and depend on nothing. */
    uint16_t hd = slot;                           /* header descriptor */
    uint16_t dd = (uint16_t)(txq.nbuf + slot);    /* data descriptor   */

    txq.desc[hd].addr  = (uint64_t)(uintptr_t)hdr;
    txq.desc[hd].len   = HDR_LEN;
    txq.desc[hd].flags = VRING_DESC_F_NEXT;
    txq.desc[hd].next  = dd;

    txq.desc[dd].addr  = (uint64_t)(uintptr_t)buf;
    txq.desc[dd].len   = len;
    txq.desc[dd].flags = 0;                   /* device-readable, end of chain */
    txq.desc[dd].next  = 0;

    txq.posted[slot] = 1;
    vq_publish(&txq, hd);
    st.tx_packets++;
    return 1;
}

uint16_t virtio_net_recv(const uint8_t **buf)
{
    if (!st.present) return 0;
    if (rxq.last_used == used_idx(&rxq)) return 0;          /* nothing new */

    struct vring_used_elem *e = &rxq.used->ring[rxq.last_used % rxq.size];
    uint16_t slot  = (uint16_t)e->id;
    uint32_t total = e->len;
    rxq.last_used++;

    if (slot >= rxq.nbuf || total <= HDR_LEN || total > BUF_SZ) {
        st.rx_dropped++;                                    /* runt or bad id */
        if (slot < rxq.nbuf) rx_post(slot);
        return 0;
    }

    uint16_t len = (uint16_t)(total - HDR_LEN);
    /* Copy out, then give the buffer straight back. Returning a pointer into a
     * buffer the device already owns again would be a race we would lose
     * intermittently and spend a day chasing. */
    memcpy(rx_scratch, rxq.bufs + (uint64_t)slot * BUF_SZ + HDR_LEN, len);
    rx_post(slot);

    *buf = rx_scratch;
    st.rx_packets++;
    return len;
}

void virtio_net_poll(void)
{
    if (!st.present) return;

    /* Reclaim transmit buffers the device has finished with. */
    while (txq.last_used != used_idx(&txq)) {
        struct vring_used_elem *e = &txq.used->ring[txq.last_used % txq.size];
        uint16_t slot = (uint16_t)e->id;          /* head == header == slot */
        if (slot < txq.nbuf) txq.posted[slot] = 0;
        txq.last_used++;
        st.tx_done++;
    }

    if (st.features & VNET_F_STATUS) {
        uint16_t s = (uint16_t)rd8(VP_CONFIG + 6) |
                     ((uint16_t)rd8(VP_CONFIG + 7) << 8);
        st.link_up = (s & 1) ? 1 : 0;
    }
}

const struct virtio_net_state *virtio_net_get(void) { return &st; }
