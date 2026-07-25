/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * nic.c — dispatch to whichever network card is present
 * ==========================================================================*/

#include "nic.h"
#include "virtio_net.h"
#include "e1000.h"
#include "serial.h"
#include "string.h"

static struct nic_info info;

static void refresh(void)
{
    if (info.kind == NIC_VIRTIO) {
        const struct virtio_net_state *v = virtio_net_get();
        info.link_up   = v->link_up;
        info.tx_packets = v->tx_packets;
        info.tx_done    = v->tx_done;
        info.rx_packets = v->rx_packets;
    } else if (info.kind == NIC_E1000) {
        const struct e1000_state *e = e1000_get();
        info.link_up   = e->link_up;
        info.tx_packets = e->tx_packets;
        info.tx_done    = e->tx_done;
        info.rx_packets = e->rx_packets;
    }
}

int nic_init(void)
{
    memset(&info, 0, sizeof(info));
    info.name = "none";

    if (virtio_net_init()) {
        const struct virtio_net_state *v = virtio_net_get();
        info.kind = NIC_VIRTIO;
        info.name = "virtio-net";
        info.present = 1;
        memcpy(info.mac, v->mac, 6);
        serial_write("[nic] using virtio-net\n");
    } else if (e1000_init()) {
        const struct e1000_state *e = e1000_get();
        info.kind = NIC_E1000;
        info.name = "e1000";
        info.present = 1;
        memcpy(info.mac, e->mac, 6);
        serial_write("[nic] using e1000 (virtio not fitted)\n");
    } else {
        serial_write("[nic] no supported network card found\n");
        return 0;
    }
    refresh();
    return 1;
}

int nic_send(const void *frame, uint16_t len)
{
    if (info.kind == NIC_VIRTIO) { int r = virtio_net_send(frame, len); refresh(); return r; }
    if (info.kind == NIC_E1000)  { int r = e1000_send(frame, len);      refresh(); return r; }
    return 0;
}

uint16_t nic_recv(const uint8_t **buf)
{
    if (info.kind == NIC_VIRTIO) { uint16_t n = virtio_net_recv(buf); if (n) refresh(); return n; }
    if (info.kind == NIC_E1000)  { uint16_t n = e1000_receive(buf);   if (n) refresh(); return n; }
    return 0;
}

void nic_poll(void)
{
    if (info.kind == NIC_VIRTIO) virtio_net_poll();
    else if (info.kind == NIC_E1000) { e1000_reap(); e1000_refresh_link(); }
    refresh();
}

int nic_wait_link(int tries)
{
    for (int i = 0; i < tries; i++) {
        nic_poll();
        if (info.link_up) return 1;
        for (volatile int d = 0; d < 200000; d++) { }
    }
    return info.link_up;
}

const struct nic_info *nic_get(void) { return &info; }
