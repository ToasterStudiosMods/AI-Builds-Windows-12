/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/reg.h — system registry
 *
 * A persistent hierarchical key/value store, kept in its own region of the disk
 * alongside the filesystem image. It is not decoration: the shell reads the
 * desktop settings out of it at boot and writes them back when they change, so
 * a chosen accent colour or wallpaper is still there after a restart.
 * ==========================================================================*/

#ifndef AURELIAN_REG_H
#define AURELIAN_REG_H

#include <stdint.h>

#define REG_MAX_KEYS    48
#define REG_MAX_VALUES  96
#define REG_NAME_MAX    24
#define REG_STR_MAX     40

enum reg_type { REG_NONE = 0, REG_DWORD = 1, REG_SZ = 2 };

/* Load the registry from disk, or seed the defaults and save them. */
void reg_mount(void);
int  reg_save(void);
int  reg_is_persistent(void);

/* --- keys --- */
int         reg_root(void);
int         reg_key_child(int parent, int i);
int         reg_key_child_count(int parent);
int         reg_key_parent(int key);
const char *reg_key_name(int key);
/* Child key called `name`, or -1. */
int         reg_key_find(int parent, const char *name);
void        reg_key_path(int key, char *buf, uint32_t cap);

/* --- values --- */
int          reg_value_count(int key);
int          reg_value_at(int key, int i);
const char  *reg_value_name(int v);
uint8_t      reg_value_type(int v);
uint32_t     reg_value_dword(int v);
const char  *reg_value_str(int v);

/* Read/write by name. The setters create the value if it is missing. */
uint32_t reg_get_dword(int key, const char *name, uint32_t fallback);
int      reg_set_dword(int key, const char *name, uint32_t val);
const char *reg_get_sz(int key, const char *name, const char *fallback);
int      reg_set_sz(int key, const char *name, const char *val);

/* Convenience: the shell's own settings key (created at mount). */
int reg_key_luma(void);

#endif /* AURELIAN_REG_H */
