/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/net.h — ethernet framing + ARP
 *
 * The first rung of the network stack: put a frame on the wire, get one back,
 * and resolve an IPv4 address to a hardware address. Everything above this
 * (IPv4, UDP, DHCP, TCP) builds on the same send/receive pair.
 * ==========================================================================*/

#ifndef AURELIAN_NET_H
#define AURELIAN_NET_H

#include <stdint.h>

struct net_state {
    uint8_t  ip[4];             /* our address (static for now)     */
    uint8_t  gw_ip[4];          /* address we are probing            */
    uint8_t  gw_mac[6];
    int      gw_resolved;
    uint32_t arp_tx, arp_rx, frames_rx, other_rx;
};

void net_init(void);
/* Broadcast an ARP request for gw_ip. */
int  net_arp_request(void);
/* Drain the receive ring and process what arrives. Call from a thread. */
void net_poll(void);

const struct net_state *net_get(void);

#endif /* AURELIAN_NET_H */
