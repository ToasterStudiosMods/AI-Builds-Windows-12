/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/nic.h — one interface, whichever card is fitted
 *
 * Two drivers exist: virtio-net (preferred — a cooperative, precisely specified
 * paravirtual device) and the Intel e1000 (real silicon, and correspondingly
 * fussier). Nothing above this header should care which one answered, so the
 * protocol code talks to a nic_* call and gets whatever is present.
 * ==========================================================================*/

#ifndef AURELIAN_NIC_H
#define AURELIAN_NIC_H

#include <stdint.h>

enum nic_kind { NIC_NONE = 0, NIC_VIRTIO, NIC_E1000 };

struct nic_info {
    enum nic_kind kind;
    const char   *name;         /* for the UI: "virtio-net" / "e1000" */
    int           present;
    int           link_up;
    uint8_t       mac[6];
    uint32_t      tx_packets;   /* frames handed to the device        */
    uint32_t      tx_done;      /* frames the device confirmed        */
    uint32_t      rx_packets;
};

/* Probe virtio first, then the Intel part. Returns 1 if a card came up. */
int      nic_init(void);
int      nic_send(const void *frame, uint16_t len);
uint16_t nic_recv(const uint8_t **buf);
/* Periodic housekeeping: reclaim sent buffers, refresh link state. */
void     nic_poll(void);
/* Wait for the link, up to `tries` polls. Returns 1 if it came up. */
int      nic_wait_link(int tries);

const struct nic_info *nic_get(void);

#endif /* AURELIAN_NIC_H */
