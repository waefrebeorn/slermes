#ifndef NOC_W12_CONFIGBACK_32E61C_H
#define NOC_W12_CONFIGBACK_32E61C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/config_backend.py (gate w12_configback_32e61c). */

/* version_token(version) -> number. version is a JSON array; the environment
 * is never touched (the live module reads no env for this function). */
json_t *w12_configback_32e61c_version_token(const json_t *version);

/* get_config_backend()/selection prefix: normalise HERMES_CONFIG_BACKEND with
 * strip().lower() (fallback "file") and classify against _KIND_RE. `env_value`
 * is an explicit seam (the Windows CRT cannot keep os.environ[k] == ""). */
json_t *w12_configback_32e61c_select(const char *env_value);

/* ConfigLockedError(path, locked_by) default message. */
json_t *w12_configback_32e61c_locked_error(const char *path, const char *locked_by);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_CONFIGBACK_32E61C_H */
