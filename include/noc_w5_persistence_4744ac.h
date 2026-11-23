#ifndef NOC_W5_PERSISTENCE_4744AC_H
#define NOC_W5_PERSISTENCE_4744AC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes persistence_assembly.py (gate w5_persistence_4744ac).
 * Only the pure derivation/decision tail is ported; store assembly, provider IO
 * and backend lifecycle stay in the Python module. Paths are injected already
 * resolved (forward-slash) by the fixture seam. */

/* PoP: w5_persistence_4744ac_store_owner_name @ persistence_assembly.py:store_owner_name
 * profile_dir/default_root are resolved absolute paths. Returns a JSON string
 * ("default" or the profile name) or JSON null. */
struct json_t *w5_persistence_4744ac_store_owner_name(const char *profile_dir,
                                                      const char *default_root);

/* PoP: w5_persistence_4744ac_state_db @ persistence_assembly.py:_state_db
 * Returns <hermes_home_key>/state.db as a JSON string. */
struct json_t *w5_persistence_4744ac_state_db(const char *hermes_home_key);

/* PoP: w5_persistence_4744ac_profile_scope @ persistence_assembly.py:profile_scope
 * Exposed seams: profile_name_for_home (string or NULL) and named_profile_home
 * (string or NULL) stand in for the two hermes_constants lookups. Returns an
 * object {"profile_dir":..., "name":...}. */
struct json_t *w5_persistence_4744ac_profile_scope(const char *home,
                                                    const char *profile,
                                                    const char *default_root,
                                                    const char *hermes_home,
                                                    const char *profile_name_for_home,
                                                    const char *named_profile_home);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_PERSISTENCE_4744AC_H */
