#ifndef NOC_W5_LOCK_32C99E_H
#define NOC_W5_LOCK_32C99E_H
#include <stddef.h>

/* Public C API for pm/lock.py (gate w5_lock_32c99e). The pure state-shape
 * transforms ({{store}} templating, artifact validation, tool history rows)
 * are ported; file IO / durable writes are outside the gate. */

/* PoP: pmlock_templatize @ pm/lock.py:_templatize
 * Returns a malloc'd JSON object string with store_root replaced by {{store}}. */
char *pmlock_templatize(const char *env_json, const char *store_root);

/* PoP: pmlock_resolve @ pm/lock.py:_resolve
 * Returns a malloc'd JSON object string with {{store}} replaced by store_root. */
char *pmlock_resolve(const char *env_json, const char *store_root);

/* PoP: pmlock_artifacts @ pm/lock.py:Lockfile.artifacts
 * Returns 1 with a malloc'd JSON array string, 0 on a malformed artifact pin
 * (Python raises ValueError). */
int pmlock_artifacts(const char *packages_json, const char *name,
                     const char *target, char **out_json);

/* PoP: pmlock_tool_rows @ pm/lock.py:_tool_rows
 * Returns a malloc'd JSON array string of the selected + previous rows. */
char *pmlock_tool_rows(const char *fact_json);

#endif /* NOC_W5_LOCK_32C99E_H */
