#ifndef HERMES_MEDIA_POLICY_H
#define HERMES_MEDIA_POLICY_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _load_gateway_cfg @ gateway/media_policy.py:_load_gateway_cfg
 * Returns the "gateway" sub-object of `config` (borrowed), or NULL.
 * (The config==NULL lazy-load fallback lives in media_policy_apply_env.) */
json_t *media_policy_load_gateway_cfg(json_t *config);

/* PoP: apply_media_policy_env @ gateway/media_policy.py:apply_media_policy_env
 * Bridge gateway media-policy settings from `config` into the environment.
 * `config` may be NULL - then the on-disk config is loaded via
 * credential_pool_load_config_safe() (the port of hermes_cli.config.load_config's
 * safe loader), mirroring Python's lazy load_config() fallback. Idempotent and
 * env-wins: a variable already present in the environment is never
 * overwritten. Never raises / never crashes on malformed config. */
void media_policy_apply_env(json_t *config);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MEDIA_POLICY_H */
