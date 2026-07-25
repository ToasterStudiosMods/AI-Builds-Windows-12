/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * ahci.c — AHCI (SATA) storage driver
 *
 * Commands are submitted through a per-port command list in DMA memory: each
 * slot points at a command table holding an ATA "register FIS" plus a scatter
 * list describing where the data should land. Writing the slot's bit into PxCI
 * hands it to the controller; the bit clears when the transfer completes.
 * ==========================================================================*/

#include "ahci.h"
#include "pci.h"
#include "mem.h"
#include "serial.h"
#include "string.h"

/* --- HBA global registers --- */
#define HBA_CAP   0x00
#define HBA_GHC   0x04
#define HBA_IS    0x08
#define HBA_PI    0x0C
#define HBA_VS    0x10

#define GHC_AE    (1u << 31)        /* AHCI enable */
#define GHC_HR    (1u << 0)         /* HBA reset   */

/* --- per-port registers, at 0x100 + port*0x80 --- */
#define PxCLB     0x00
#define PxCLBU    0x04
#define PxFB      0x08
#define PxFBU     0x0C
#define PxIS      0x10
#define PxIE      0x14
#define PxCMD     0x18
#define PxTFD     0x20
#define PxSIG     0x24
#define PxSSTS    0x28
#define PxSERR    0x30
#define PxCI      0x38

#define CMD_ST    (1u << 0)         /* start          */
#define CMD_FRE   (1u << 4)         /* FIS receive on */
#define CMD_FR    (1u << 14)        /* FIS rx running */
#define CMD_CR    (1u << 15)        /* cmd list running */

#define TFD_BSY   (1u << 7)
#define TFD_DRQ   (1u << 3)
#define TFD_ERR   (1u << 0)

#define SIG_SATA   0x00000101u
#define SIG_SATAPI 0xEB140101u

#define ATA_IDENTIFY      0xEC
#define ATA_READ_DMA_EXT  0x25

struct cmd_header {
    uint16_t flags;                 /* 0-4 CFL, 5 A, 6 W, 7 P, 8 R, 9 B, 10 C */
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba, ctbau;
    uint32_t rsv[4];
} __attribute__((packed));

struct prdt_entry {
    uint32_t dba, dbau, rsv;
    uint32_t dbc;                   /* bits 0-21 byte count - 1, bit 31 IOC */
} __attribute__((packed));

struct cmd_table {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    struct prdt_entry prdt[8];
} __attribute__((packed));

static struct ahci_state st;
static struct cmd_header *clist;    /* 32 slots, 1 KiB, 1 KiB aligned */
static struct cmd_table  *ctab;     /* 32 tables                      */
static uint8_t           *fisbase;  /* 256 bytes, 256-byte aligned    */
static uint8_t           *dmabuf;   /* one sector of scratch          */
static uint8_t            first_sector[16];

static inline void wr(uint32_t off, uint32_t v)
{ *(volatile uint32_t *)(uintptr_t)(st.abar + off) = v; }
static inline uint32_t rd(uint32_t off)
{ return *(volatile uint32_t *)(uintptr_t)(st.abar + off); }

static inline uint32_t port_off(uint8_t p, uint32_t reg)
{ return 0x100u + (uint32_t)p * 0x80u + reg; }
static inline void pwr(uint8_t p, uint32_t reg, uint32_t v) { wr(port_off(p, reg), v); }
static inline uint32_t prd(uint8_t p, uint32_t reg) { return rd(port_off(p, reg)); }

/* Same cache problem as the NIC: registers must not be write-back cached. */
static void map_uncached(uint64_t phys)
{
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFull);
    uint64_t e = pml4[(phys >> 39) & 0x1FF];
    if (!(e & 1)) return;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(e & ~0xFFFull);
    e = pdpt[(phys >> 30) & 0x1FF];
    if (!(e & 1)) return;
    uint64_t *pd = (uint64_t *)(uintptr_t)(e & ~0xFFFull);
    uint64_t i = (phys >> 21) & 0x1FF;
    if (!(pd[i] & 1)) return;
    pd[i] |= (1ull << 4) | (1ull << 3);
    __asm__ volatile ("invlpg (%0)" :: "r"((uintptr_t)phys) : "memory");
}

static void port_stop(uint8_t p)
{
    pwr(p, PxCMD, prd(p, PxCMD) & ~CMD_ST);
    pwr(p, PxCMD, prd(p, PxCMD) & ~CMD_FRE);
    for (int i = 0; i < 500000; i++)
        if (!(prd(p, PxCMD) & (CMD_FR | CMD_CR))) break;
}

static void port_start(uint8_t p)
{
    for (int i = 0; i < 500000; i++)
        if (!(prd(p, PxCMD) & CMD_CR)) break;
    pwr(p, PxCMD, prd(p, PxCMD) | CMD_FRE);
    pwr(p, PxCMD, prd(p, PxCMD) | CMD_ST);
}

/* Issue slot 0 and wait for it to retire. 1 = success. */
static int run_slot0(uint8_t p)
{
    pwr(p, PxIS, 0xFFFFFFFFu);
    for (int i = 0; i < 1000000; i++)
        if (!(prd(p, PxTFD) & (TFD_BSY | TFD_DRQ))) break;

    pwr(p, PxCI, 1u);
    for (int i = 0; i < 5000000; i++) {
        if (!(prd(p, PxCI) & 1u)) break;
        if (prd(p, PxIS) & (1u << 30)) return 0;        /* task file error */
    }
    if (prd(p, PxCI) & 1u) return 0;                    /* timed out */
    if (prd(p, PxTFD) & TFD_ERR) return 0;
    return 1;
}

/* Build the command table for a single-buffer transfer. */
static void setup_slot0(void *buf, uint32_t bytes, int write)
{
    memset(&clist[0], 0, sizeof(struct cmd_header));
    clist[0].flags = (5u & 0x1F) | (write ? (1u << 6) : 0u);  /* CFL=5 dwords */
    clist[0].prdtl = 1;
    clist[0].ctba  = (uint32_t)((uint64_t)(uintptr_t)&ctab[0] & 0xFFFFFFFFu);
    clist[0].ctbau = (uint32_t)((uint64_t)(uintptr_t)&ctab[0] >> 32);

    memset(&ctab[0], 0, sizeof(struct cmd_table));
    ctab[0].prdt[0].dba  = (uint32_t)((uint64_t)(uintptr_t)buf & 0xFFFFFFFFu);
    ctab[0].prdt[0].dbau = (uint32_t)((uint64_t)(uintptr_t)buf >> 32);
    ctab[0].prdt[0].dbc  = (bytes - 1) | (1u << 31);
}

static int identify(uint8_t p)
{
    setup_slot0(dmabuf, 512, 0);
    uint8_t *f = ctab[0].cfis;
    f[0] = 0x27;                    /* host-to-device register FIS */
    f[1] = 0x80;                    /* command, not control        */
    f[2] = ATA_IDENTIFY;
    if (!run_slot0(p)) return 0;

    const uint16_t *id = (const uint16_t *)dmabuf;
    /* Model name lives in words 27..46 as byte-swapped ASCII. */
    for (int i = 0; i < 20; i++) {
        st.disk.model[i * 2 + 0] = (char)(id[27 + i] >> 8);
        st.disk.model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    st.disk.model[40] = 0;
    for (int i = 39; i >= 0 && st.disk.model[i] == ' '; i--) st.disk.model[i] = 0;

    /* Word 83 bit 10 marks LBA48 support; capacity is words 100..103. */
    if (id[83] & (1u << 10)) {
        st.disk.sectors = (uint64_t)id[100] | ((uint64_t)id[101] << 16) |
                          ((uint64_t)id[102] << 32) | ((uint64_t)id[103] << 48);
    } else {
        st.disk.sectors = (uint64_t)id[60] | ((uint64_t)id[61] << 16);
    }
    return 1;
}

int ahci_read(uint64_t lba, uint32_t count, void *buf)
{
    if (!st.present || !st.disk.present || count == 0 || count > 8) return 0;

    setup_slot0(buf, count * AHCI_SECTOR, 0);
    uint8_t *f = ctab[0].cfis;
    f[0] = 0x27;
    f[1] = 0x80;
    f[2] = ATA_READ_DMA_EXT;
    f[4] = (uint8_t)(lba      );
    f[5] = (uint8_t)(lba >>  8);
    f[6] = (uint8_t)(lba >> 16);
    f[7] = 0x40;                    /* LBA mode */
    f[8] = (uint8_t)(lba >> 24);
    f[9] = (uint8_t)(lba >> 32);
    f[10] = (uint8_t)(lba >> 40);
    f[12] = (uint8_t)(count      );
    f[13] = (uint8_t)(count >> 8);

    if (!run_slot0(st.disk.port)) { st.disk.reads_failed++; return 0; }
    st.disk.reads_ok++;
    return 1;
}

int ahci_init(void)
{
    memset(&st, 0, sizeof(st));

    /* Find a SATA controller: class 1 (mass storage), subclass 6 (SATA). */
    int idx = -1;
    for (int i = 0; i < pci_count(); i++) {
        const struct pci_dev *d = pci_get(i);
        if (d->class_code == 0x01 && d->subclass == 0x06) { idx = i; break; }
    }
    if (idx < 0) { serial_write("[ahci] no SATA controller\n"); return 0; }

    const struct pci_dev *d = pci_get(idx);
    uint64_t abar = d->bar[5] & ~0xFull;        /* ABAR is always BAR5 */
    if (!abar) { serial_write("[ahci] ABAR not mapped\n"); return 0; }
    st.abar = abar;
    map_uncached(abar);

    wr(HBA_GHC, rd(HBA_GHC) | GHC_AE);          /* make sure AHCI mode is on */
    st.ports_implemented = rd(HBA_PI);
    serial_write("[ahci] abar "); serial_write_hex(abar);
    serial_write(" ports "); serial_write_hex(st.ports_implemented);
    serial_write("\n");

    clist   = (struct cmd_header *)phys_alloc(32 * sizeof(struct cmd_header));
    ctab    = (struct cmd_table  *)phys_alloc(32 * sizeof(struct cmd_table));
    fisbase = (uint8_t *)phys_alloc(4096);
    dmabuf  = (uint8_t *)phys_alloc(4096);
    if (!clist || !ctab || !fisbase || !dmabuf) {
        serial_write("[ahci] DMA allocation failed\n");
        return 0;
    }
    st.present = 1;

    for (uint8_t p = 0; p < AHCI_MAX_PORTS; p++) {
        if (!(st.ports_implemented & (1u << p))) continue;

        uint32_t ssts = prd(p, PxSSTS);
        if ((ssts & 0x0F) != 3) continue;               /* no device present */
        st.port_count++;

        uint32_t sig  = prd(p, PxSIG);
        uint8_t  kind = (sig == SIG_SATA)   ? AHCI_SATA
                      : (sig == SIG_SATAPI) ? AHCI_SATAPI : AHCI_OTHER;

        serial_write("[ahci] port "); serial_write_u64(p);
        serial_write(kind == AHCI_SATA   ? ": SATA disk\n"
                   : kind == AHCI_SATAPI ? ": ATAPI (optical)\n"
                                         : ": unknown device\n");

        /* Only drive real disks; optical media needs SCSI packet commands. */
        if (kind != AHCI_SATA || st.disk.present) continue;

        port_stop(p);
        pwr(p, PxCLB,  (uint32_t)((uint64_t)(uintptr_t)clist & 0xFFFFFFFFu));
        pwr(p, PxCLBU, (uint32_t)((uint64_t)(uintptr_t)clist >> 32));
        pwr(p, PxFB,   (uint32_t)((uint64_t)(uintptr_t)fisbase & 0xFFFFFFFFu));
        pwr(p, PxFBU,  (uint32_t)((uint64_t)(uintptr_t)fisbase >> 32));
        pwr(p, PxSERR, 0xFFFFFFFFu);
        pwr(p, PxIE, 0);                                /* we poll */
        port_start(p);

        st.disk.port = p;
        st.disk.kind = kind;
        if (!identify(p)) {
            serial_write("[ahci] IDENTIFY failed\n");
            port_stop(p);
            continue;
        }
        st.disk.present = 1;
        serial_write("[ahci] model '"); serial_write(st.disk.model);
        serial_write("' sectors "); serial_write_u64(st.disk.sectors);
        serial_write("\n");

        /* Prove the read path works by pulling LBA 0 and keeping its first
         * bytes, which the Devices page shows for comparison against the
         * image that was actually attached. */
        if (ahci_read(0, 1, dmabuf)) {
            memcpy(first_sector, dmabuf, sizeof(first_sector));
            serial_write("[ahci] LBA0 reads ");
            for (int i = 0; i < 4; i++) { serial_write_hex(first_sector[i]); serial_write(" "); }
            serial_write("\n");
        } else {
            serial_write("[ahci] LBA0 read FAILED\n");
        }
    }

    if (!st.disk.present)
        serial_write("[ahci] no usable SATA disk (optical media is not driven yet)\n");
    return 1;
}

const struct ahci_state *ahci_get(void) { return &st; }
const uint8_t *ahci_first_sector(void)  { return first_sector; }
