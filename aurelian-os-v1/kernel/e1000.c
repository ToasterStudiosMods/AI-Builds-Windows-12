/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * e1000.c — Intel 8254x gigabit ethernet driver (polled)
 * ==========================================================================*/

#include "e1000.h"
#include "pci.h"
#include "mem.h"
#include "serial.h"
#include "string.h"

/* --- register offsets --- */
#define REG_CTRL      0x0000
#define REG_STATUS    0x0008
#define REG_EERD      0x0014
#define REG_ICR       0x00C0
#define REG_IMC       0x00D8
#define REG_RCTL      0x0100
#define REG_TCTL      0x0400
#define REG_RDBAL     0x2800
#define REG_RDBAH     0x2804
#define REG_RDLEN     0x2808
#define REG_RDH       0x2810
#define REG_RDT       0x2818
#define REG_TDBAL     0x3800
#define REG_TDBAH     0x3804
#define REG_TDLEN     0x3808
#define REG_TDH       0x3810
#define REG_TDT       0x3818
#define REG_RAL0      0x5400
#define REG_RAH0      0x5404

#define CTRL_SLU      (1u << 6)     /* set link up            */
#define CTRL_ASDE     (1u << 5)     /* auto-speed detect      */
#define STATUS_LU     (1u << 1)     /* link up                */

#define RCTL_EN       (1u << 1)
#define RCTL_BAM      (1u << 15)    /* accept broadcast       */
#define RCTL_SECRC    (1u << 26)    /* strip ethernet CRC     */
#define RCTL_SZ_2048  0             /* BSIZE=00 -> 2048 bytes */

#define TCTL_EN       (1u << 1)
#define TCTL_PSP      (1u << 3)     /* pad short packets      */

#define TXD_CMD_EOP   (1u << 0)
#define TXD_CMD_IFCS  (1u << 1)
#define TXD_CMD_RS    (1u << 3)
#define TXD_STAT_DD   (1u << 0)

#define RXD_STAT_DD   (1u << 0)
#define RXD_STAT_EOP  (1u << 1)

#define NRX 32
#define NTX 16
#define BUFSZ 2048

struct rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

struct tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

static struct e1000_state st;
static volatile struct rx_desc *rxd;
static volatile struct tx_desc *txd;
static uint8_t *rxbuf, *txbuf;
static uint32_t rx_next, tx_next;

/* --- MMIO accessors. The window is mapped uncached (see map_uncached). --- */
static inline void wr(uint32_t off, uint32_t v)
{ *(volatile uint32_t *)(uintptr_t)(st.mmio + off) = v; }
static inline uint32_t rd(uint32_t off)
{ return *(volatile uint32_t *)(uintptr_t)(st.mmio + off); }

/* Device registers must not be cached, but the boot page tables map the low
 * 4 GiB as ordinary write-back memory with 2 MiB pages. Walk the tables from
 * CR3 and set PCD/PWT on the entry covering the register window, otherwise
 * reads can be served from cache and the driver sees stale values. */
static void map_uncached(uint64_t phys)
{
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFull);
    uint64_t pml4e = pml4[(phys >> 39) & 0x1FF];
    if (!(pml4e & 1)) return;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4e & ~0xFFFull);
    uint64_t pdpte = pdpt[(phys >> 30) & 0x1FF];
    if (!(pdpte & 1)) return;
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpte & ~0xFFFull);
    uint64_t i = (phys >> 21) & 0x1FF;
    if (!(pd[i] & 1)) return;
    pd[i] |= (1ull << 4) | (1ull << 3);            /* PCD | PWT */
    __asm__ volatile ("invlpg (%0)" :: "r"((uintptr_t)phys) : "memory");
}

static int read_mac(void)
{
    /* Prefer the receive address registers: firmware normally programs them,
     * and it avoids depending on the EEPROM interface. */
    uint32_t low = rd(REG_RAL0), high = rd(REG_RAH0);
    if (low || (high & 0xFFFF)) {
        st.mac[0] = (uint8_t)(low      );
        st.mac[1] = (uint8_t)(low >>  8);
        st.mac[2] = (uint8_t)(low >> 16);
        st.mac[3] = (uint8_t)(low >> 24);
        st.mac[4] = (uint8_t)(high     );
        st.mac[5] = (uint8_t)(high >> 8);
        return 1;
    }
    /* Fall back to EEPROM words 0..2 (each holds two MAC bytes). */
    for (int w = 0; w < 3; w++) {
        wr(REG_EERD, ((uint32_t)w << 8) | 1u);      /* address + START */
        uint32_t v = 0;
        for (int spin = 0; spin < 100000; spin++) {
            v = rd(REG_EERD);
            if (v & (1u << 4)) break;               /* DONE */
        }
        if (!(v & (1u << 4))) return 0;
        st.mac[w * 2 + 0] = (uint8_t)(v >> 16);
        st.mac[w * 2 + 1] = (uint8_t)(v >> 24);
    }
    return 1;
}

void e1000_refresh_link(void)
{
    if (st.present) st.link_up = (rd(REG_STATUS) & STATUS_LU) ? 1 : 0;
}

int e1000_init(void)
{
    memset(&st, 0, sizeof(st));

    int idx = pci_find_network();
    if (idx < 0) { serial_write("[e1000] no network controller\n"); return 0; }
    const struct pci_dev *d = pci_get(idx);
    if (d->vendor != 0x8086) {
        serial_write("[e1000] network card is not Intel; unsupported\n");
        return 0;
    }
    /* BAR0 is a 32-bit memory BAR; the low four bits are flags. */
    uint64_t bar = d->bar[0] & ~0xFull;
    if (!bar || (d->bar[0] & 1)) {
        serial_write("[e1000] BAR0 is not a memory window\n");
        return 0;
    }
    st.mmio = bar;
    map_uncached(bar);

    serial_write("[e1000] mmio "); serial_write_hex(bar); serial_write("\n");

    wr(REG_IMC, 0xFFFFFFFFu);                  /* mask all interrupts: we poll */
    rd(REG_ICR);

    if (!read_mac()) { serial_write("[e1000] could not read MAC\n"); return 0; }
    serial_write("[e1000] mac ");
    for (int i = 0; i < 6; i++) { serial_write_hex(st.mac[i]); serial_write(i < 5 ? ":" : "\n"); }

    /* Descriptor rings and packet buffers. phys_alloc is page aligned, which
     * more than satisfies the 16-byte descriptor alignment requirement. */
    rxd   = (volatile struct rx_desc *)phys_alloc(NRX * sizeof(struct rx_desc));
    txd   = (volatile struct tx_desc *)phys_alloc(NTX * sizeof(struct tx_desc));
    rxbuf = (uint8_t *)phys_alloc(NRX * BUFSZ);
    txbuf = (uint8_t *)phys_alloc(NTX * BUFSZ);
    if (!rxd || !txd || !rxbuf || !txbuf) {
        serial_write("[e1000] ring allocation failed\n");
        return 0;
    }
    memset((void *)rxd, 0, NRX * sizeof(struct rx_desc));
    memset((void *)txd, 0, NTX * sizeof(struct tx_desc));

    for (int i = 0; i < NRX; i++) {
        rxd[i].addr   = (uint64_t)(uintptr_t)(rxbuf + (uint64_t)i * BUFSZ);
        rxd[i].status = 0;
    }
    for (int i = 0; i < NTX; i++) {
        txd[i].addr   = (uint64_t)(uintptr_t)(txbuf + (uint64_t)i * BUFSZ);
        txd[i].status = TXD_STAT_DD;           /* free */
    }

    /* Receive ring */
    wr(REG_RDBAL, (uint32_t)((uint64_t)(uintptr_t)rxd & 0xFFFFFFFFu));
    wr(REG_RDBAH, (uint32_t)((uint64_t)(uintptr_t)rxd >> 32));
    wr(REG_RDLEN, NRX * sizeof(struct rx_desc));
    wr(REG_RDH, 0);
    wr(REG_RDT, NRX - 1);
    wr(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC | RCTL_SZ_2048);

    /* Transmit ring */
    wr(REG_TDBAL, (uint32_t)((uint64_t)(uintptr_t)txd & 0xFFFFFFFFu));
    wr(REG_TDBAH, (uint32_t)((uint64_t)(uintptr_t)txd >> 32));
    wr(REG_TDLEN, NTX * sizeof(struct tx_desc));
    wr(REG_TDH, 0);
    wr(REG_TDT, 0);
    wr(REG_TCTL, TCTL_EN | TCTL_PSP | (0x10 << 4) | (0x40 << 12));

    /* Bring the link up and let the PHY negotiate. */
    wr(REG_CTRL, rd(REG_CTRL) | CTRL_SLU | CTRL_ASDE);

    rx_next = 0;
    tx_next = 0;
    st.present = 1;
    e1000_refresh_link();

    serial_write("[e1000] ready, link ");
    serial_write(st.link_up ? "up\n" : "down (negotiating)\n");
    return 1;
}

int e1000_send(const void *frame, uint16_t len)
{
    if (!st.present || len == 0 || len > BUFSZ) return 0;

    volatile struct tx_desc *t = &txd[tx_next];
    if (!(t->status & TXD_STAT_DD)) return 0;          /* ring full */

    memcpy(txbuf + (uint64_t)tx_next * BUFSZ, frame, len);
    t->length = len;
    t->cso    = 0;
    t->cmd    = TXD_CMD_EOP | TXD_CMD_IFCS | TXD_CMD_RS;
    t->status = 0;

    tx_next = (tx_next + 1) % NTX;
    wr(REG_TDT, tx_next);                              /* hand it to the card */
    st.tx_packets++;
    return 1;
}

uint16_t e1000_receive(const uint8_t **buf)
{
    if (!st.present) return 0;

    volatile struct rx_desc *r = &rxd[rx_next];
    if (!(r->status & RXD_STAT_DD)) return 0;          /* nothing new */

    uint16_t len = r->length;
    if (r->errors) { st.rx_errors++; len = 0; }
    else           { *buf = rxbuf + (uint64_t)rx_next * BUFSZ; st.rx_packets++; }
    st.last_rx_len = len;

    r->status = 0;                                     /* return to the card */
    wr(REG_RDT, rx_next);
    rx_next = (rx_next + 1) % NRX;
    return len;
}

const struct e1000_state *e1000_get(void) { return &st; }
