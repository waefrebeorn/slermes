#ifndef NOC_W17_PROFILEMOD_D242F9_H
#define NOC_W17_PROFILEMOD_D242F9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_profilemod_d242f9_requested @ hermes_cli/profile_model_seed.py:_requested_provider
 * model.provider is a string; the oracle stubs _expand_env_vars as identity. */
struct json_t *w17_profilemod_d242f9_requested(const struct json_t *model_cfg);

/* PoP: w17_profilemod_d242f9_aliases @ hermes_cli/profile_model_seed.py:_aliases
 * Sorted identity aliases for an entry: {name or provider_key, provider_key} minus
 * empties. The oracle stubs custom_provider_aliases and _expand_env_vars. */
struct json_t *w17_profilemod_d242f9_aliases(const struct json_t *entry, const char *key);

/* PoP: w17_profilemod_d242f9_merge @ hermes_cli/profile_model_seed.py:_merge_endpoint_seed
 * Return a copy of dst with seed endpoint written first; same-identity dst
 * entries (those intersecting `claimed`) are dropped in both sections. claimed
 * is a JSON array of identity strings. */
struct json_t *w17_profilemod_d242f9_merge(const struct json_t *dst,
                                           const struct json_t *seed,
                                           const struct json_t *claimed);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PROFILEMOD_D242F9_H */
