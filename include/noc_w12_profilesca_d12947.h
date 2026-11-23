#ifndef NOC_W12_PROFILESCAD12947_H
#define NOC_W12_PROFILESCAD12947_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_profilesca_d12947_cached_profile_read @ hermes_cli/profiles_cache.py:_cached_profile_read
 *
 * Faithful port of the derived-value cache decision.  `signature` is the value
 * signature_of(path) would return (NULL / JSON null == missing file, never
 * cached); `compute_value` is the value compute() would return.  `cache_max`
 * mirrors _PROFILE_FILE_CACHE_MAX and `reset` clears the whole cache first.
 * Returns a fresh object {"value": <derived>, "computed": bool, "cleared": bool}.
 * `cleared` is true when the max-size guard emptied the cache this call.
 * Caller frees the returned tree. */
struct json_t *w12_profilesca_d12947_cached_profile_read(
    const char *path, const char *kind,
    const struct json_t *signature, const struct json_t *compute_value,
    long long cache_max, int reset);

/* PoP: w12_profilesca_d12947_yaml_scalar_to_json @ hermes_cli/profiles_cache.py:yaml_scalar_to_json
 * json.dumps `default` hook: isoformat() when callable else str(value).
 * The JSON-representable scalars are covered: string -> itself, number ->
 * str(), bool -> "True"/"False", null -> "None".  Caller supplies the buffer. */
void w12_profilesca_d12947_yaml_scalar_to_json(const struct json_t *value,
                                               char *out, size_t cap);

/* PoP: w12_profilesca_d12947_json_safe_dumps @ hermes_cli/profiles_cache.py:json_safe_dumps
 * json.dumps(value, default=..., allow_nan=False) formatting (Python default
 * separators ", " / ": "), or "{}" on failure.  The literal string
 * "__W12_NAN__" is an injected seam standing in for a non-finite float (the
 * oracle maps it to float('nan')); it triggers the allow_nan=False fallback.
 * Returns a malloc'd string; caller frees. */
char *w12_profilesca_d12947_json_safe_dumps(const struct json_t *value);

/* Release the module-level cache (process teardown). */
void w12_profilesca_d12947_cache_free(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_PROFILESCAD12947_H */
