#ifndef NOC_W5_STORE_63BE39_H
#define NOC_W5_STORE_63BE39_H
#include <stddef.h>

/* C home for upstream Hermes pm/store.py (gate w5_store_63be39).
 * Only the pure validation / target-decision tail is ported; the download,
 * extraction, digest and lock IO in the Python module stays out of the gate. */

/* PoP: w5_store_63be39_entry_component @ pm/store.py:entry_component
 * Returns 1 and a malloc'd copy of name on success (caller frees); 0 when
 * Python would raise ValueError (single-path-component guard). */
int w5_store_63be39_entry_component(const char *name, char **out_name);

/* PoP: w5_store_63be39_fetch_entry_name @ pm/store.py:fetch_entry_name
 * Returns 1 and a malloc'd "fetch-<sha256>" on a lowercase 64-hex digest;
 * 0 when Python would raise ValueError. */
int w5_store_63be39_fetch_entry_name(const char *digest, char **out_name);

/* PoP: w5_store_63be39_current_target @ pm/store.py:current_target
 * Pure tail of current_target: the three OS probes (_native_machine,
 * _is_bionic_libc, _is_musl_libc) are injected by the gate. sys_platform is
 * sys.platform. Returns a malloc'd target triple, or NULL when Python would
 * raise RuntimeError (unsupported architecture). */
char *w5_store_63be39_current_target(const char *machine, const char *sys_platform,
                                     int bionic, int musl);

/* PoP: w5_store_63be39_free @ pm/store.py:<module> — free a returned string. */
void w5_store_63be39_free(char *s);

#endif /* NOC_W5_STORE_63BE39_H */
