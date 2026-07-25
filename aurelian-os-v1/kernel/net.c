/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * net.c — ethernet framing and ARP
 * ==========================================================================*/

#include "net.h"
#include "e1000.h"
#include "serial.h"
#include "string.h"

#define ETH_ARP  0x0806
#define ETH_IPV4 0x0800

struct eth_hdr {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;              /* big endian */
} __attribute__((packed));

struct arp_pkt {
    uint16_t htype, ptype;
    uint8_t  hlen, plen;
    uint16_t oper;
    uint8_t  sha[6], spa[4];
    uint8_t  tha[6], tpa[4];
} __attribute__((packed));

static struct net_state ns;

static uint16_t hton16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
#define ntoh16 hton16

void net_init(void)
{
    memset(&ns, 0, sizeof(ns));
    /* VirtualBox NAT hands out 10.0.2.15 with the gateway at 10.0.2.2. We have
     * no DHCP client yet, so use those as a static configuration — enough to
     * prove the wire works end to end. */
    ns.ip[0] = 10; ns.ip[1] = 0; ns.ip[2] = 2; ns.ip[3] = 15;
    ns.gw_ip[0] = 10; ns.gw_ip[1] = 0; ns.gw_ip[2] = 2; ns.gw_ip[3] = 2;
    serial_write("[net] 10.0.2.15, probing gateway 10.0.2.2\n");
}

int net_arp_request(void)
{
    const struct e1000_state *e = e1000_get();
    if (!e->present) return 0;

    uint8_t frame[sizeof(struct eth_hdr) + sizeof(struct arp_pkt)];
    memset(frame, 0, sizeof(frame));

    struct eth_hdr *eh = (struct eth_hdr *)frame;
    struct arp_pkt *ap = (struct arp_pkt *)(frame + sizeof(struct eth_hdr));

    for (int i = 0; i < 6; i++) eh->dst[i] = 0xFF;         /* broadcast */
    memcpy(eh->src, e->mac, 6);
    eh->type = hton16(ETH_ARP);

    ap->htype = hton16(1);                                  /* ethernet */
    ap->ptype = hton16(ETH_IPV4);
    ap->hlen  = 6;
    ap->plen  = 4;
    ap->oper  = hton16(1);                                  /* request  */
    memcpy(ap->sha, e->mac, 6);
    memcpy(ap->spa, ns.ip, 4);
    memcpy(ap->tpa, ns.gw_ip, 4);

    if (!e1000_send(frame, sizeof(frame))) return 0;
    ns.arp_tx++;
    return 1;
}

static void handle_arp(const uint8_t *payload, uint16_t len)
{
    if (len < sizeof(struct arp_pkt)) return;
    const struct arp_pkt *ap = (const struct arp_pkt *)payload;
    if (ntoh16(ap->oper) != 2) return;                      /* want a reply */
    if (memcmp(ap->spa, ns.gw_ip, 4) != 0) return;          /* from the gateway? */

    memcpy(ns.gw_mac, ap->sha, 6);
    ns.gw_resolved = 1;
    ns.arp_rx++;

    serial_write("[net] ARP reply: 10.0.2.2 is at ");
    for (int i = 0; i < 6; i++) {
        serial_write_hex8(ns.gw_mac[i]);
        serial_write(i < 5 ? ":" : "\n");
    }
}

void net_poll(void)
{
    const uint8_t *buf = 0;
    for (int guard = 0; guard < 32; guard++) {
        uint16_t len = e1000_receive(&buf);
        if (!len || !buf) return;
        ns.frames_rx++;
        if (len < sizeof(struct eth_hdr)) continue;

        const struct eth_hdr *eh = (const struct eth_hdr *)buf;
        uint16_t type = ntoh16(eh->type);
        if (type == ETH_ARP)
            handle_arp(buf + sizeof(struct eth_hdr),
                       (uint16_t)(len - sizeof(struct eth_hdr)));
        else
            ns.other_rx++;
    }
}

const struct net_state *net_get(void) { return &ns; }
