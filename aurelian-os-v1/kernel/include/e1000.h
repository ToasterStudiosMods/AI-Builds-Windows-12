/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/e1000.h — Intel 8254x (82540EM) gigabit ethernet driver
 *
 * The card is found on the PCI bus, its register window is mapped uncached,
 * and packets move through DMA descriptor rings in physical memory. Because the
 * boot page tables identity-map the low 4 GiB, a pointer *is* its own physical
 * address here, which keeps the DMA setup honest and simple.
 * ==========================================================================*/

#ifndef AURELIAN_E1000_H
#define AURELIAN_E1000_H

#include <stdint.h>

struct e1000_state {
    int      present;
    int      link_up;
    uint64_t mmio;              /* register window (physical == virtual) */
    uint8_t  mac[6];
    uint32_t tx_packets, tx_done, tx_deferred, rx_packets, rx_errors;
    uint16_t last_rx_len;
};

/* Locate and bring up the card. Returns 1 on success. */
int  e1000_init(void);
/* Transmit one raw ethernet frame. Returns 1 if it was queued. */
int  e1000_send(const void *frame, uint16_t len);
/* Pull one received frame, if any: returns its length and sets *buf. */
uint16_t e1000_receive(const uint8_t **buf);
/* Refresh the cached link state. */
void e1000_refresh_link(void);
/* Account for completed transmits (call periodically, not from send). */
void e1000_reap(void);
/* Poll until the link comes up (or `tries` attempts elapse). */
int  e1000_wait_link(int tries);

const struct e1000_state *e1000_get(void);

#endif /* AURELIAN_E1000_H */
