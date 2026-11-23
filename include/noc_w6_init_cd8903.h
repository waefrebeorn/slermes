#ifndef NOC_W6_INIT_CD8903_H
#define NOC_W6_INIT_CD8903_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: init_host_claims @ providers/__init__.py:_host_claims
 * JSON bool: whether `claimed` (str/strip/lower/rstrip('.')) matches `host`
 * exactly or as a parent domain. Caller frees. */
json_t *init_host_claims(const char *host, const char *claimed);

/* PoP: init_module_name @ providers/__init__.py:_user_module_name
 * Returns "_hermes_user_provider_<sha1(home_key)[:10]>_<name with '-' -> '_'>".
 * SHA-1 is computed in-process (no libcrypto dependency). Caller frees. */
json_t *init_module_name(const char *name, const char *home_key);

/* PoP: init_stamps_settled @ providers/__init__.py:_plugin_dir_stamps_settled
 * `facts` supplies "now" (int64 decimal string) and "stamps" (array of null or
 * int64 decimal strings). True iff every stamp is at least one 2e9 ns mtime
 * tick from `now`. `now` is injected so the port is deterministic. Caller frees. */
json_t *init_stamps_settled(const json_t *facts);

#ifdef __cplusplus
}
#endif
#endif
