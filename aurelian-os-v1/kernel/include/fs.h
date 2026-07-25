/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/fs.h — tiny in-memory filesystem (ramfs)
 *
 * A flat node table with parent links, seeded at boot with a small tree. It
 * backs the File Explorer and Notepad. Nodes are referenced by index; the root
 * directory is index 0. Files have a fixed capacity so edits happen in place
 * (no allocator in v1).
 * ==========================================================================*/

#ifndef AURELIAN_FS_H
#define AURELIAN_FS_H

#include <stdint.h>

#define FS_MAX_NODES 64
#define FS_NAME_MAX  24

void        fs_init(void);
int         fs_root(void);
/* i-th child of `dir`, or -1 when there are no more. */
int         fs_child(int dir, int i);
int         fs_child_count(int dir);
const char *fs_name(int n);
int         fs_is_dir(int n);
int         fs_parent(int n);
uint32_t    fs_size(int n);
uint32_t    fs_capacity(int n);
const char *fs_read(int n);
/* Overwrite a file's contents (clamped to its capacity). Returns bytes kept. */
uint32_t    fs_write(int n, const char *data, uint32_t len);
/* Build "/Documents/readme.txt" into buf. */
void        fs_path(int n, char *buf, uint32_t cap);
/* Create an empty file in `dir`; returns its index or -1. */
int         fs_create(int dir, const char *name);

#endif /* AURELIAN_FS_H */
