/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/pci.h — PCI configuration space enumeration
 *
 * Discovers devices on the PCI buses through the legacy 0xCF8/0xCFC config
 * ports. This is the groundwork every real device driver needs: to talk to a
 * network card we first have to find it, learn its BARs (where its registers
 * are mapped) and its interrupt line.
 * ==========================================================================*/

#ifndef AURELIAN_PCI_H
#define AURELIAN_PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 32

struct pci_dev {
    uint8_t  bus, slot, func;
    uint16_t vendor, device;
    uint8_t  class_code, subclass, prog_if, revision;
    uint32_t bar[6];
    uint8_t  irq_line;
};

/* Scan bus 0..7 and fill `out`. Returns the number of devices found. */
int pci_scan(void);
int pci_count(void);
const struct pci_dev *pci_get(int i);

/* Human-readable names (a small built-in table; unknown ids show as hex). */
const char *pci_class_name(uint8_t class_code, uint8_t subclass);
const char *pci_vendor_name(uint16_t vendor);
/* Known device name, or 0 when we have no entry for it. */
const char *pci_device_name(uint16_t vendor, uint16_t device);

/* Enable a device's decoders and bus mastering. Required before any DMA. */
void pci_enable(const struct pci_dev *d);

/* Index of the first network controller (class 0x02), or -1. */
int pci_find_network(void);

#endif /* AURELIAN_PCI_H */
