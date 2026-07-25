/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/virtio_net.h — virtio-net driver (legacy virtio-pci)
 *
 * Chosen over the Intel part because the protocol is far simpler and precisely
 * specified: two split-ring virtqueues, a descriptor table, an available ring
 * the driver writes and a used ring the device writes. There is no vendor
 * erratum to guess at — the rules for when a buffer becomes visible are stated.
 * ==========================================================================*/

#ifndef AURELIAN_VIRTIO_NET_H
#define AURELIAN_VIRTIO_NET_H

#include <stdint.h>

struct virtio_net_state {
    int      present;
    int      link_up;
    uint16_t iobase;            /* legacy virtio-pci uses an I/O BAR */
    uint8_t  mac[6];
    uint32_t features;          /* what we negotiated                */
    uint16_t rxq_size, txq_size;
    uint32_t tx_packets, tx_done, rx_packets, rx_dropped;
};

int      virtio_net_init(void);
int      virtio_net_send(const void *frame, uint16_t len);
/* Returns the length of a received frame and points *buf at it, or 0. */
uint16_t virtio_net_recv(const uint8_t **buf);
/* Recycle completed transmit buffers and top the receive queue back up. */
void     virtio_net_poll(void);

const struct virtio_net_state *virtio_net_get(void);

#endif /* AURELIAN_VIRTIO_NET_H */
