#ifndef NOC_PROFILECACHE_H
#define NOC_PROFILECACHE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: profile_slot_get @ hermes_cli/models_profile_cache.py:profile_slot_get
 * PoP: profile_slot_set @ hermes_cli/models_profile_cache.py:profile_slot_set
 *
 * Drives one module's slots with injected get_hermes_home_override() /
 * hermes_home_key() per operation. `initial` seeds module attributes; `ops`
 * are {"op":"set"|"get","attr":...,"value"/"default":...,
 *        "override": <string|null>, "home": <string>}.
 * Returns a JSON array with one result per "get" op. */
struct json_t *profilecache_run(const struct json_t *initial,
                                const struct json_t *ops);

#ifdef __cplusplus
}
#endif

#endif /* NOC_PROFILECACHE_H */
