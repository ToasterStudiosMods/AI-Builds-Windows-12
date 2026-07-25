/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * fs.c — tiny in-memory filesystem (ramfs)
 * ==========================================================================*/

#include "fs.h"
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

static struct node nodes[FS_MAX_NODES];
static char        pool[POOL_SIZE];
static uint32_t    pool_used;
static int         node_count;

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
        "[ ] disk driver\n"
        "[ ] real filesystem\n"
        "[ ] processes\n");

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
        "This is NOT a real system folder.\n\n"
        "There is no disk driver and no real\n"
        "filesystem yet. Every folder and file\n"
        "you can see is a small tree built in RAM\n"
        "at boot by kernel/fs.c - it does not\n"
        "contain the operating system, and it is\n"
        "not stored on the disc you booted from.\n\n"
        "Edits are kept in memory only and are\n"
        "gone on the next restart.\n\n"
        "Real storage needs, in order:\n"
        "  1. PCI enumeration        (done)\n"
        "  2. AHCI/SATA disk driver  (todo)\n"
        "  3. a real filesystem      (todo)\n");
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
