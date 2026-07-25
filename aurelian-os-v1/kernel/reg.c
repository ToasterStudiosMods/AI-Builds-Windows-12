/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * reg.c — system registry, persisted to its own region of the disk
 * ==========================================================================*/

#include "reg.h"
#include "ahci.h"
#include "pci.h"
#include "serial.h"
#include "string.h"

struct reg_key_rec {
    char    name[REG_NAME_MAX];
    int16_t parent;
    uint8_t used;
    uint8_t pad;
};

struct reg_val_rec {
    char     name[REG_NAME_MAX];
    int16_t  key;
    uint8_t  type;
    uint8_t  used;
    uint32_t dword;
    char     str[REG_STR_MAX];
};

static struct reg_key_rec keys[REG_MAX_KEYS];
static struct reg_val_rec vals[REG_MAX_VALUES];
static int  nkeys, nvals;
static int  persistent;
static int  key_luma = -1;

/* ---------- helpers ---------- */
static void copy_name(char *dst, const char *src, uint32_t cap)
{
    uint32_t i = 0;
    while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int name_eq(const char *a, const char *b)
{ while (*a && *a == *b) { a++; b++; } return *a == *b; }

static int add_key(int parent, const char *name)
{
    if (nkeys >= REG_MAX_KEYS) return -1;
    struct reg_key_rec *k = &keys[nkeys];
    copy_name(k->name, name, REG_NAME_MAX);
    k->parent = (int16_t)parent;
    k->used   = 1;
    return nkeys++;
}

static int add_val(int key, const char *name, uint8_t type)
{
    if (nvals >= REG_MAX_VALUES) return -1;
    struct reg_val_rec *v = &vals[nvals];
    memset(v, 0, sizeof(*v));
    copy_name(v->name, name, REG_NAME_MAX);
    v->key  = (int16_t)key;
    v->type = type;
    v->used = 1;
    return nvals++;
}

static int find_val(int key, const char *name)
{
    for (int i = 0; i < nvals; i++)
        if (vals[i].used && vals[i].key == key && name_eq(vals[i].name, name))
            return i;
    return -1;
}

/* ---------- keys ---------- */
int reg_root(void) { return 0; }

int reg_key_child(int parent, int i)
{
    int seen = 0;
    for (int k = 0; k < nkeys; k++) {
        if (!keys[k].used || keys[k].parent != parent) continue;
        if (seen == i) return k;
        seen++;
    }
    return -1;
}

int reg_key_child_count(int parent)
{
    int n = 0;
    for (int k = 0; k < nkeys; k++)
        if (keys[k].used && keys[k].parent == parent) n++;
    return n;
}

int reg_key_parent(int key)
{ return (key >= 0 && key < nkeys) ? keys[key].parent : -1; }

const char *reg_key_name(int key)
{ return (key >= 0 && key < nkeys) ? keys[key].name : ""; }

int reg_key_find(int parent, const char *name)
{
    for (int k = 0; k < nkeys; k++)
        if (keys[k].used && keys[k].parent == parent && name_eq(keys[k].name, name))
            return k;
    return -1;
}

void reg_key_path(int key, char *buf, uint32_t cap)
{
    int chain[12], depth = 0;
    while (key > 0 && depth < 12) { chain[depth++] = key; key = keys[key].parent; }
    uint32_t p = 0;
    if (cap > 8) { const char *r = "AURELIAN"; while (*r && p + 1 < cap) buf[p++] = *r++; }
    for (int i = depth - 1; i >= 0; i--) {
        if (p + 1 < cap) buf[p++] = '\\';
        const char *s = keys[chain[i]].name;
        for (uint32_t j = 0; s[j] && p + 1 < cap; j++) buf[p++] = s[j];
    }
    buf[p] = 0;
}

/* ---------- values ---------- */
int reg_value_count(int key)
{
    int n = 0;
    for (int i = 0; i < nvals; i++) if (vals[i].used && vals[i].key == key) n++;
    return n;
}

int reg_value_at(int key, int i)
{
    int seen = 0;
    for (int v = 0; v < nvals; v++) {
        if (!vals[v].used || vals[v].key != key) continue;
        if (seen == i) return v;
        seen++;
    }
    return -1;
}

const char *reg_value_name(int v)
{ return (v >= 0 && v < nvals) ? vals[v].name : ""; }
uint8_t reg_value_type(int v)
{ return (v >= 0 && v < nvals) ? vals[v].type : REG_NONE; }
uint32_t reg_value_dword(int v)
{ return (v >= 0 && v < nvals) ? vals[v].dword : 0; }
const char *reg_value_str(int v)
{ return (v >= 0 && v < nvals) ? vals[v].str : ""; }

uint32_t reg_get_dword(int key, const char *name, uint32_t fallback)
{
    int v = find_val(key, name);
    return (v >= 0 && vals[v].type == REG_DWORD) ? vals[v].dword : fallback;
}

int reg_set_dword(int key, const char *name, uint32_t val)
{
    int v = find_val(key, name);
    if (v < 0) v = add_val(key, name, REG_DWORD);
    if (v < 0) return 0;
    vals[v].type  = REG_DWORD;
    vals[v].dword = val;
    return 1;
}

const char *reg_get_sz(int key, const char *name, const char *fallback)
{
    int v = find_val(key, name);
    return (v >= 0 && vals[v].type == REG_SZ) ? vals[v].str : fallback;
}

int reg_set_sz(int key, const char *name, const char *val)
{
    int v = find_val(key, name);
    if (v < 0) v = add_val(key, name, REG_SZ);
    if (v < 0) return 0;
    vals[v].type = REG_SZ;
    copy_name(vals[v].str, val, REG_STR_MAX);
    return 1;
}

int reg_key_luma(void) { return key_luma; }
int reg_is_persistent(void) { return persistent; }

/* ---------- default content ---------- */
static void seed(void)
{
    nkeys = nvals = 0;
    memset(keys, 0, sizeof(keys));
    memset(vals, 0, sizeof(vals));

    int root = add_key(-1, "ROOT");

    int sys = add_key(root, "SYSTEM");
    int cv  = add_key(sys,  "CurrentVersion");
    reg_set_sz(cv, "ProductName",  "Aurelian OS");
    reg_set_sz(cv, "Codename",     "Luma");
    reg_set_sz(cv, "KernelName",   "Aurelion");
    reg_set_sz(cv, "Version",      "1.0.0-dev");
    reg_set_sz(cv, "Arch",         "x86-64");
    reg_set_dword(cv, "BuildNumber", 1);

    int boot = add_key(sys, "Boot");
    reg_set_sz(boot, "Loader",     "GRUB Multiboot2");
    reg_set_sz(boot, "Filesystem", "AURFS1");

    int sw   = add_key(root, "SOFTWARE");
    key_luma = add_key(sw,   "Luma");
    /* Defaults matching the shell's own initial state. */
    reg_set_dword(key_luma, "AccentColour", 0x0078D4);
    reg_set_dword(key_luma, "DarkMode",     0);
    reg_set_dword(key_luma, "Wallpaper",    0);
    reg_set_sz(key_luma,    "Shell",        "Luma");
    reg_set_sz(key_luma,    "Theme",        "Prism");

    int hw = add_key(root, "HARDWARE");
    int dev = add_key(hw, "Devices");
    reg_set_dword(dev, "PciDeviceCount", (uint32_t)pci_count());
    int nic = pci_find_network();
    reg_set_sz(dev, "NetworkCard", nic >= 0
               ? (pci_device_name(pci_get(nic)->vendor, pci_get(nic)->device)
                  ? pci_device_name(pci_get(nic)->vendor, pci_get(nic)->device)
                  : "unknown")
               : "none");
    const struct ahci_state *ah = ahci_get();
    reg_set_sz(dev, "Disk", (ah->present && ah->disk.present) ? ah->disk.model : "none");
    reg_set_dword(dev, "DiskSectors", (uint32_t)(ah->disk.present ? ah->disk.sectors : 0));
}

/* ---------- persistence ---------- */
#define REG_MAGIC0 'A'
#define REG_MAGIC1 'R'
#define REG_MAGIC2 'E'
#define REG_MAGIC3 'G'
#define REG_VERSION 1

struct reg_super {
    char     magic[4];
    uint32_t version;
    uint32_t nkeys, nvals;
    uint32_t keys_bytes, vals_bytes;
    int32_t  key_luma;
};

#define REG_IMAGE_BYTES (AHCI_REG_SECTORS * AHCI_SECTOR)
static uint8_t image[REG_IMAGE_BYTES];

int reg_save(void)
{
    const struct ahci_state *ah = ahci_get();
    if (!ah->present || !ah->disk.present) return 0;
    if (sizeof(struct reg_super) + sizeof(keys) + sizeof(vals) > REG_IMAGE_BYTES) {
        serial_write("[reg] image too large\n");
        return 0;
    }
    memset(image, 0, sizeof(image));
    struct reg_super *sb = (struct reg_super *)image;
    sb->magic[0] = REG_MAGIC0; sb->magic[1] = REG_MAGIC1;
    sb->magic[2] = REG_MAGIC2; sb->magic[3] = REG_MAGIC3;
    sb->version    = REG_VERSION;
    sb->nkeys      = (uint32_t)nkeys;
    sb->nvals      = (uint32_t)nvals;
    sb->keys_bytes = (uint32_t)sizeof(keys);
    sb->vals_bytes = (uint32_t)sizeof(vals);
    sb->key_luma   = (int32_t)key_luma;

    uint8_t *p = image + sizeof(struct reg_super);
    memcpy(p, keys, sizeof(keys)); p += sizeof(keys);
    memcpy(p, vals, sizeof(vals));

    for (uint32_t s = 0; s < AHCI_REG_SECTORS; s += 8) {
        uint32_t n = (AHCI_REG_SECTORS - s) < 8 ? (AHCI_REG_SECTORS - s) : 8;
        if (!ahci_write(AHCI_REG_LBA + s, n, image + (uint64_t)s * AHCI_SECTOR))
            return 0;
    }
    return 1;
}

static int reg_load(void)
{
    const struct ahci_state *ah = ahci_get();
    if (!ah->present || !ah->disk.present) return 0;

    for (uint32_t s = 0; s < AHCI_REG_SECTORS; s += 8) {
        uint32_t n = (AHCI_REG_SECTORS - s) < 8 ? (AHCI_REG_SECTORS - s) : 8;
        if (!ahci_read(AHCI_REG_LBA + s, n, image + (uint64_t)s * AHCI_SECTOR))
            return 0;
    }
    const struct reg_super *sb = (const struct reg_super *)image;
    if (sb->magic[0] != REG_MAGIC0 || sb->magic[1] != REG_MAGIC1 ||
        sb->magic[2] != REG_MAGIC2 || sb->magic[3] != REG_MAGIC3) return 0;
    if (sb->version != REG_VERSION) return 0;
    if (sb->keys_bytes != sizeof(keys) || sb->vals_bytes != sizeof(vals)) return 0;
    if (sb->nkeys > REG_MAX_KEYS || sb->nvals > REG_MAX_VALUES) return 0;

    const uint8_t *p = image + sizeof(struct reg_super);
    memcpy(keys, p, sizeof(keys)); p += sizeof(keys);
    memcpy(vals, p, sizeof(vals));
    nkeys    = (int)sb->nkeys;
    nvals    = (int)sb->nvals;
    key_luma = (int)sb->key_luma;
    return 1;
}

void reg_mount(void)
{
    if (reg_load()) {
        persistent = 1;
        serial_write("[reg] loaded from disk, ");
        serial_write_u64((uint64_t)nkeys); serial_write(" keys, ");
        serial_write_u64((uint64_t)nvals); serial_write(" values\n");
        return;
    }
    seed();
    persistent = reg_save();
    serial_write(persistent ? "[reg] seeded and saved to disk\n"
                            : "[reg] no disk; registry is in memory only\n");
}
