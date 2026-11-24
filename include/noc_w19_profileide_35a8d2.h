/* noc_w19_profileide_35a8d2.h - public C API for persistence/sqlite/housekeeping/profile_identity.py
 * port. The repository's SQL bodies stay Python-side; the pure session-key/receipt rekey
 * decision helpers are ported here. */
#ifndef NOC_W19_PROFILEIDE_35A8D2_H
#define NOC_W19_PROFILEIDE_35A8D2_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_profileide_35a8d2_moved_key @ persistence/sqlite/housekeeping/profile_identity.py:_moved_key
 * `key` moved from the "agent:<old>:" session-key namespace to "agent:<new>:", or NULL when it is
 * not a JSON string in that namespace. Returns a malloc'd string (caller frees). */
char *w19_profileide_35a8d2_moved_key(const json_t *key, const char *old_name, const char *new_name);

/* PoP: w19_profileide_35a8d2_rekey_entry @ persistence/sqlite/housekeeping/profile_identity.py:_rekey_entry
 * Move one serialized SessionEntry from profile `old_name` to `new_name`: its "agent:<old>:"
 * session_key namespace and its origin.profile. `entry` is not mutated; the reply carries a copy.
 * Returns {"changed": bool, "entry": <entry copy>}. */
json_t *w19_profileide_35a8d2_rekey_entry(const json_t *entry, const char *old_name, const char *new_name);

/* PoP: w19_profileide_35a8d2_rekey_allowed @ persistence/sqlite/housekeeping/profile_identity.py:rekey_profile
 * The rekey_profile guard: strip both names and refuse an empty or identical pair. */
int w19_profileide_35a8d2_rekey_allowed(const char *old_name, const char *new_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PROFILEIDE_35A8D2_H */
