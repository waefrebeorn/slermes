#ifndef NOC_W6_UVCACHEHEA_D7F380_H
#define NOC_W6_UVCACHEHEA_D7F380_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: uvcachehea_normalize @ pm/uv_cache_health.py:normalize
 * PEP 503 normal form: re.sub(r"[-_.]+", "-", name).lower(). Caller frees. */
json_t *uvcachehea_normalize(const char *name);

/* PoP: uvcachehea_valid @ pm/uv_cache_health.py:_valid
 * Returns the normalized name when _NAME.fullmatch(name), else JSON null. */
json_t *uvcachehea_valid(const char *name);

/* PoP: uvcachehea_failed @ pm/uv_cache_health.py:failed_cached_packages
 * Returns the sorted array of PEP 503 names uv could not install from a
 * corrupt cached wheel (the Python set, order made deterministic). */
json_t *uvcachehea_failed(const char *output);

/* PoP: uvcachehea_dist_name @ pm/uv_cache_health.py:_dist_name
 * Returns the validated normalized project name of a *.dist-info dir, else
 * JSON null. Caller frees. */
json_t *uvcachehea_dist_name(const char *info);

#ifdef __cplusplus
}
#endif
#endif
