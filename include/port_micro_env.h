/*
 * port_micro_env.h — micro pure helpers, one unit:
 *   agent/terminal_env_provider.py  strip_env_keys, env_description (Base)
 *   gateway/platforms/access_policy_mixin.py _allow_all_env_names
 *   gateway/host_topology.py        _normalized (delegates to the proven
 *                                   profile normalization)
 */
#ifndef PORT_MICRO_ENV_H
#define PORT_MICRO_ENV_H

/* Base.strip_env_keys(): the empty frozenset. Returns "[]". */
char *tep_strip_env_keys(void);

/* Base.env_description(display_name): "a {name} environment (likely Linux)". */
char *tep_env_description(const char *display_name);

/* access_policy_mixin._allow_all_env_names(prefix): JSON array
 * ["GATEWAY_ALLOW_ALL_USERS", "{prefix}_ALLOW_ALL_USERS"]. */
char *apm_allow_all_env_names(const char *prefix);

/* host_topology._normalized(name): normalize_profile_name(name), or "" for a
 * falsy name. Lowercases (after strip) and maps any casing of "default" to
 * "default"; the ValueError path (empty after strip) returns "" defensively.
 * Returns malloc'd, caller frees. */
char *ht_normalized(const char *name);

#endif /* PORT_MICRO_ENV_H */
