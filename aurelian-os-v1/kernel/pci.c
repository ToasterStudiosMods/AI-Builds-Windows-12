/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * pci.c — PCI configuration space enumeration (legacy 0xCF8/0xCFC mechanism)
 * ==========================================================================*/

#include "pci.h"
#include "io.h"
#include "serial.h"

#define CONFIG_ADDR 0xCF8
#define CONFIG_DATA 0xCFC

static struct pci_dev devs[PCI_MAX_DEVICES];
static int ndev;

static uint32_t cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    uint32_t addr = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11)
                  | ((uint32_t)func << 8) | (off & 0xFCu);
    outl(CONFIG_ADDR, addr);
    return inl(CONFIG_DATA);
}

static void probe(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint32_t id = cfg_read32(bus, slot, func, 0x00);
    uint16_t vendor = (uint16_t)(id & 0xFFFF);
    if (vendor == 0xFFFF || vendor == 0x0000) return;      /* nothing there */
    if (ndev >= PCI_MAX_DEVICES) return;

    struct pci_dev *d = &devs[ndev++];
    d->bus = bus; d->slot = slot; d->func = func;
    d->vendor = vendor;
    d->device = (uint16_t)(id >> 16);

    uint32_t cls = cfg_read32(bus, slot, func, 0x08);
    d->revision   = (uint8_t)(cls & 0xFF);
    d->prog_if    = (uint8_t)((cls >> 8) & 0xFF);
    d->subclass   = (uint8_t)((cls >> 16) & 0xFF);
    d->class_code = (uint8_t)((cls >> 24) & 0xFF);

    for (int i = 0; i < 6; i++)
        d->bar[i] = cfg_read32(bus, slot, func, (uint8_t)(0x10 + i * 4));

    d->irq_line = (uint8_t)(cfg_read32(bus, slot, func, 0x3C) & 0xFF);
}

int pci_scan(void)
{
    ndev = 0;
    for (uint16_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = cfg_read32((uint8_t)bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) continue;
            probe((uint8_t)bus, slot, 0);
            /* multi-function device? bit 7 of the header type */
            uint32_t hdr = cfg_read32((uint8_t)bus, slot, 0, 0x0C);
            if ((hdr >> 16) & 0x80)
                for (uint8_t f = 1; f < 8; f++)
                    probe((uint8_t)bus, slot, f);
        }
    }
    serial_write("[pci] "); serial_write_u64((uint64_t)ndev);
    serial_write(" devices\n");
    for (int i = 0; i < ndev; i++) {
        const struct pci_dev *d = &devs[i];
        serial_write("  ");
        serial_write_hex(d->vendor); serial_write(":");
        serial_write_hex(d->device); serial_write("  class ");
        serial_write_u64(d->class_code); serial_write(".");
        serial_write_u64(d->subclass); serial_write("  ");
        serial_write(pci_class_name(d->class_code, d->subclass));
        serial_write("\n");
    }
    return ndev;
}

int pci_count(void) { return ndev; }

const struct pci_dev *pci_get(int i)
{ return (i >= 0 && i < ndev) ? &devs[i] : 0; }

int pci_find_network(void)
{
    for (int i = 0; i < ndev; i++)
        if (devs[i].class_code == 0x02) return i;
    return -1;
}

const char *pci_class_name(uint8_t c, uint8_t s)
{
    switch (c) {
    case 0x00: return "Unclassified";
    case 0x01:
        switch (s) {
        case 0x01: return "IDE controller";
        case 0x06: return "SATA controller";
        case 0x08: return "NVMe controller";
        default:   return "Storage controller";
        }
    case 0x02: return (s == 0x00) ? "Ethernet controller" : "Network controller";
    case 0x03: return "VGA display";
    case 0x04: return (s == 0x01) ? "Audio device" : "Multimedia";
    case 0x06:
        switch (s) {
        case 0x00: return "Host bridge";
        case 0x01: return "ISA bridge";
        case 0x04: return "PCI bridge";
        default:   return "Bridge";
        }
    case 0x07: return "Serial controller";
    case 0x08: return "System peripheral";
    case 0x09: return "Input device";
    case 0x0C:
        switch (s) {
        case 0x03: return "USB controller";
        case 0x05: return "SMBus";
        default:   return "Serial bus";
        }
    default: return "Device";
    }
}

const char *pci_vendor_name(uint16_t v)
{
    switch (v) {
    case 0x8086: return "Intel";
    case 0x1022: return "AMD";
    case 0x10DE: return "NVIDIA";
    case 0x1002: return "ATI/AMD";
    case 0x1AF4: return "Red Hat (virtio)";
    case 0x80EE: return "Oracle VirtualBox";
    case 0x15AD: return "VMware";
    case 0x1234: return "QEMU";
    case 0x1013: return "Cirrus Logic";
    case 0x106B: return "Apple";
    default:     return "Unknown vendor";
    }
}

const char *pci_device_name(uint16_t v, uint16_t d)
{
    if (v == 0x8086) {
        switch (d) {
        case 0x100E: return "82540EM Gigabit Ethernet";
        case 0x10D3: return "82574L Gigabit Ethernet";
        case 0x153A: return "I217-LM Ethernet";
        case 0x7000: return "PIIX3 ISA bridge";
        case 0x7010: return "PIIX3 IDE";
        case 0x7113: return "PIIX4 ACPI";
        case 0x1237: return "440FX host bridge";
        case 0x2668: return "ICH6 audio";
        case 0x2829: return "ICH9 SATA (AHCI)";
        case 0x2918: return "ICH9 LPC";
        case 0x293E: return "ICH9 HD audio";
        default: return 0;
        }
    }
    if (v == 0x80EE) {
        switch (d) {
        case 0xBEEF: return "VirtualBox graphics adapter";
        case 0xCAFE: return "VirtualBox guest service";
        default: return 0;
        }
    }
    if (v == 0x1AF4) {
        switch (d) {
        case 0x1000: return "virtio network device";
        case 0x1001: return "virtio block device";
        default: return 0;
        }
    }
    return 0;
}
