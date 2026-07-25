/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/ahci.h — AHCI (SATA) storage driver
 *
 * The route to real, persistent files. The controller is found on the PCI bus,
 * its register window mapped uncached, and commands are issued through DMA
 * command lists exactly as the AHCI specification describes. Reading LBA 0 off
 * a real disk is the milestone that makes a filesystem possible.
 * ==========================================================================*/

#ifndef AURELIAN_AHCI_H
#define AURELIAN_AHCI_H

#include <stdint.h>

#define AHCI_MAX_PORTS 32
#define AHCI_SECTOR    512

enum ahci_kind { AHCI_NONE = 0, AHCI_SATA, AHCI_SATAPI, AHCI_OTHER };

struct ahci_disk {
    int      present;
    uint8_t  port;
    uint8_t  kind;                  /* enum ahci_kind          */
    char     model[41];             /* from IDENTIFY DEVICE    */
    uint64_t sectors;               /* LBA48 capacity          */
    uint32_t reads_ok, reads_failed;
};

struct ahci_state {
    int      present;
    uint64_t abar;
    uint32_t ports_implemented;
    int      port_count;            /* ports with something attached */
    struct ahci_disk disk;          /* the first usable SATA disk    */
};

int  ahci_init(void);
/* Read `count` sectors from `lba` into buf (count * 512 bytes). 1 on success. */
int  ahci_read(uint64_t lba, uint32_t count, void *buf);

const struct ahci_state *ahci_get(void);
/* First 16 bytes of LBA 0, captured at init as proof of a real read. */
const uint8_t *ahci_first_sector(void);

#endif /* AURELIAN_AHCI_H */
