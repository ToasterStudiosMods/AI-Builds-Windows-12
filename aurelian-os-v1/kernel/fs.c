/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * fs.c — tiny in-memory filesystem (ramfs)
 * ==========================================================================*/

#include "fs.h"
#include "ahci.h"
#include "serial.h"
#include "string.h"

#define POOL_SIZE   12288
#define FILE_CAP      768   /* per-file capacity carved out of the pool */

struct node {
    char     name[FS_NAME_MAX];
    uint8_t  used;
    uint8_t  is_dir;
    int16_t  parent;
    uint32_t off;      /* offset into the content pool (files only) */
    uint32_t len;
    uint32_t cap;
};

struct node nodes[FS_MAX_NODES];
char        pool[POOL_SIZE];
uint32_t    pool_used;
int         node_count;
int         fs_persistent;   /* tree is backed by a disk image */
uint32_t    fs_boot_count;   /* incremented and re-saved every mount */

static void set_name(char *dst, const char *src)
{
    uint32_t i = 0;
    while (src[i] && i < FS_NAME_MAX - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int add_node(int parent, const char *name, int is_dir, const char *body)
{
    if (node_count >= FS_MAX_NODES) return -1;
    struct node *n = &nodes[node_count];
    set_name(n->name, name);
    n->used   = 1;
    n->is_dir = (uint8_t)is_dir;
    n->parent = (int16_t)parent;
    n->off = n->len = n->cap = 0;

    if (!is_dir) {
        if (pool_used + FILE_CAP > POOL_SIZE) return -1;
        n->off = pool_used;
        n->cap = FILE_CAP;
        pool_used += FILE_CAP;
        uint32_t i = 0;
        /* leave room for the NUL fs_read() writes at off+len */
        if (body) while (body[i] && i < n->cap - 1) { pool[n->off + i] = body[i]; i++; }
        n->len = i;
    }
    return node_count++;
}

void fs_init(void)
{
    node_count = 0;
    pool_used  = 0;
    memset(nodes, 0, sizeof(nodes));

    int root = add_node(-1, "/", 1, 0);

    int docs = add_node(root, "Documents", 1, 0);
    int pics = add_node(root, "Pictures",  1, 0);
    int mus  = add_node(root, "Music",     1, 0);
    int dl   = add_node(root, "Downloads", 1, 0);
    int sys  = add_node(root, "System",    1, 0);

    add_node(docs, "readme.txt", 0,
        "Welcome to Aurelian OS.\n\n"
        "This is a from-scratch x86-64 operating\n"
        "system with its own kernel (Aurelion),\n"
        "graphics stack and desktop shell (Luma).\n\n"
        "Everything you see is drawn by the\n"
        "kernel itself - there is no Linux or\n"
        "Windows underneath.\n\n"
        "Edit this file and press Save.\n");
    add_node(docs, "notes.txt", 0, "Scratch notes.\n\n- \n");
    add_node(docs, "todo.txt",  0,
        "Roadmap\n"
        "-------\n"
        "[x] framebuffer graphics\n"
        "[x] interrupts + PS/2 input\n"
        "[x] Luma Shell + apps\n"
        "[x] AHCI disk driver, read and write\n"
        "[x] files survive a restart\n"
        "[ ] registry + editor\n"
        "[ ] networking\n"
        "[ ] processes with address spaces\n");

    add_node(pics, "wallpapers.txt", 0,
        "Wallpapers ship as boot modules and are\n"
        "selectable in Settings > Personalisation.\n");

    add_node(mus, "playlist.txt", 0, "Nothing here yet - no audio driver.\n");
    add_node(dl,  "welcome.txt",  0, "Downloads folder.\n");

    add_node(sys, "version.txt", 0,
        "Aurelian OS 1.0.0-dev\n"
        "kernel: Aurelion (x86-64)\n"
        "shell:  Luma\n"
        "ui:     Prism\n");
    add_node(sys, "license.txt", 0,
        "Original clean-room implementation.\n"
        "No Microsoft code or assets.\n");
    add_node(sys, "READ-THIS.txt", 0,
        "About this filesystem.\n\n"
        "With a SATA disk attached, this tree is\n"
        "stored on it and your edits survive a\n"
        "restart. Explorer says which mode is in\n"
        "use at the top of the listing, and\n"
        "Settings > System reports the details.\n\n"
        "With no disk it runs in memory only and\n"
        "resets on every boot.\n\n"
        "It still does not contain the operating\n"
        "system itself - that lives on the boot\n"
        "media, not in here.\n");
}

int fs_root(void) { return 0; }

int fs_child(int dir, int i)
{
    int seen = 0;
    for (int k = 0; k < node_count; k++) {
        if (!nodes[k].used || nodes[k].parent != dir) continue;
        if (seen == i) return k;
        seen++;
    }
    return -1;
}

int fs_child_count(int dir)
{
    int n = 0;
    for (int k = 0; k < node_count; k++)
        if (nodes[k].used && nodes[k].parent == dir) n++;
    return n;
}

const char *fs_name(int n)
{ return (n >= 0 && n < node_count) ? nodes[n].name : ""; }

int fs_is_dir(int n)
{ return (n >= 0 && n < node_count) ? nodes[n].is_dir : 0; }

int fs_parent(int n)
{ return (n >= 0 && n < node_count) ? nodes[n].parent : -1; }

uint32_t fs_size(int n)
{ return (n >= 0 && n < node_count) ? nodes[n].len : 0; }

uint32_t fs_capacity(int n)
{ return (n >= 0 && n < node_count) ? nodes[n].cap : 0; }

const char *fs_read(int n)
{
    if (n < 0 || n >= node_count || nodes[n].is_dir) return "";
    pool[nodes[n].off + nodes[n].len] = 0;   /* cap leaves room for the NUL */
    return &pool[nodes[n].off];
}

uint32_t fs_write(int n, const char *data, uint32_t len)
{
    if (n < 0 || n >= node_count || nodes[n].is_dir) return 0;
    if (len > nodes[n].cap - 1) len = nodes[n].cap - 1;
    for (uint32_t i = 0; i < len; i++) pool[nodes[n].off + i] = data[i];
    nodes[n].len = len;
    pool[nodes[n].off + len] = 0;
    return len;
}

void fs_path(int n, char *buf, uint32_t cap)
{
    /* walk to the root collecting names, then emit in reverse */
    int chain[16], depth = 0;
    while (n > 0 && depth < 16) { chain[depth++] = n; n = nodes[n].parent; }
    uint32_t p = 0;
    if (depth == 0) { if (cap > 1) buf[p++] = '/'; buf[p] = 0; return; }
    for (int i = depth - 1; i >= 0; i--) {
        if (p + 1 < cap) buf[p++] = '/';
        const char *s = nodes[chain[i]].name;
        for (uint32_t k = 0; s[k] && p + 1 < cap; k++) buf[p++] = s[k];
    }
    buf[p] = 0;
}

int fs_create(int dir, const char *name)
{
    return add_node(dir, name, 0, "");
}


/* ==================================================================
 * Persistence
 *
 * The whole tree is small and fixed-size, so it is written out verbatim: a
 * superblock sector carrying a magic and the counts, followed by the node table
 * and the content pool. That is enough for files edited in Notepad to still be
 * there after a restart, which is the point of having a disk at all.
 *
 * The layout deliberately lives above AHCI_RESERVED_LBA so it can never scribble
 * on a disk that already holds something.
 * ================================================================== */

#define FS_MAGIC0 'A'
#define FS_MAGIC1 'U'
#define FS_MAGIC2 'R'
#define FS_MAGIC3 'F'
#define FS_VERSION 1

struct fs_super {
    char     magic[4];          /* AURF */
    uint32_t version;
    uint32_t node_count;
    uint32_t pool_used;
    uint32_t nodes_bytes;
    uint32_t pool_bytes;
    uint32_t boot_count;        /* proves a modified image really persisted */
};

/* One staging buffer, sector aligned in size, covering super + nodes + pool. */
#define FS_IMAGE_BYTES (AHCI_FS_SECTORS * AHCI_SECTOR)
static uint8_t fs_image[FS_IMAGE_BYTES];

int fs_disk_save(void)
{
    const struct ahci_state *ah = ahci_get();
    if (!ah->present || !ah->disk.present) return 0;

    uint32_t need = (uint32_t)(sizeof(struct fs_super) + sizeof(nodes) + sizeof(pool));
    if (need > FS_IMAGE_BYTES) { serial_write("[fs] image too large to persist\n"); return 0; }

    memset(fs_image, 0, sizeof(fs_image));
    struct fs_super *sb = (struct fs_super *)fs_image;
    sb->magic[0] = FS_MAGIC0; sb->magic[1] = FS_MAGIC1;
    sb->magic[2] = FS_MAGIC2; sb->magic[3] = FS_MAGIC3;
    sb->version     = FS_VERSION;
    sb->node_count  = (uint32_t)node_count;
    sb->pool_used   = pool_used;
    sb->nodes_bytes = (uint32_t)sizeof(nodes);
    sb->pool_bytes  = (uint32_t)sizeof(pool);
    sb->boot_count  = fs_boot_count;

    uint8_t *p = fs_image + sizeof(struct fs_super);
    memcpy(p, nodes, sizeof(nodes)); p += sizeof(nodes);
    memcpy(p, pool,  sizeof(pool));

    /* ahci_write takes at most 8 sectors per command. */
    for (uint32_t s = 0; s < AHCI_FS_SECTORS; s += 8) {
        uint32_t n = (AHCI_FS_SECTORS - s) < 8 ? (AHCI_FS_SECTORS - s) : 8;
        if (!ahci_write(AHCI_FS_LBA + s, n, fs_image + (uint64_t)s * AHCI_SECTOR)) {
            serial_write("[fs] save failed\n");
            return 0;
        }
    }
    return 1;
}

int fs_disk_load(void)
{
    const struct ahci_state *ah = ahci_get();
    if (!ah->present || !ah->disk.present) return 0;

    for (uint32_t s = 0; s < AHCI_FS_SECTORS; s += 8) {
        uint32_t n = (AHCI_FS_SECTORS - s) < 8 ? (AHCI_FS_SECTORS - s) : 8;
        if (!ahci_read(AHCI_FS_LBA + s, n, fs_image + (uint64_t)s * AHCI_SECTOR))
            return 0;
    }

    const struct fs_super *sb = (const struct fs_super *)fs_image;
    if (sb->magic[0] != FS_MAGIC0 || sb->magic[1] != FS_MAGIC1 ||
        sb->magic[2] != FS_MAGIC2 || sb->magic[3] != FS_MAGIC3) return 0;
    if (sb->version != FS_VERSION) return 0;
    if (sb->nodes_bytes != sizeof(nodes) || sb->pool_bytes != sizeof(pool)) return 0;
    if (sb->node_count > FS_MAX_NODES) return 0;

    const uint8_t *p = fs_image + sizeof(struct fs_super);
    memcpy(nodes, p, sizeof(nodes)); p += sizeof(nodes);
    memcpy(pool,  p, sizeof(pool));
    node_count    = (int)sb->node_count;
    pool_used     = sb->pool_used;
    fs_boot_count = sb->boot_count;

    serial_write("[fs] loaded from disk, ");
    serial_write_u64((uint64_t)node_count);
    serial_write(" nodes\n");
    return 1;
}

/* Load the tree from disk if a valid image is there, otherwise build the
 * defaults and write them out so the next boot finds them. */
void fs_mount(void)
{
    if (fs_disk_load()) {
        fs_persistent = 1;
        /* Bump the counter and write it straight back. If this number climbs
         * across restarts then a *modified* image is genuinely reaching the
         * disk, which is a stronger claim than reading back what we wrote in
         * the same session. */
        fs_boot_count++;
        fs_disk_save();
        serial_write("[fs] mounted from disk, boot #");
        serial_write_u64((uint64_t)fs_boot_count);
        serial_write("\n");
        return;
    }
    fs_init();
    fs_boot_count = 1;
    fs_persistent = fs_disk_save();
    serial_write(fs_persistent ? "[fs] formatted disk, boot #1\n"
                               : "[fs] no disk; running in memory only\n");
}
