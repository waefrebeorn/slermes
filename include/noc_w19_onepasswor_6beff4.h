#ifndef NOC_W19_ONEPASSWOR_6BEFF4_H
#define NOC_W19_ONEPASSWOR_6BEFF4_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_onepasswor_6beff4_all_origins @ agent/vault_backends/onepassword.py:_all_origins
 * Every normalized origin of the item's urls, deduped, order preserved; unparsable
 * urls are skipped exactly as the live `except Exception: continue` does. */
json_t *w19_onepasswor_6beff4_all_origins(json_t *urls);

/* PoP: w19_onepasswor_6beff4_web_origins @ agent/vault_backends/onepassword.py:_web_origins
 * The http/https fill targets, or the first origin alone when none is a web page. */
json_t *w19_onepasswor_6beff4_web_origins(json_t *origins);

/* PoP: w19_onepasswor_6beff4_item_selector @ agent/vault_backends/onepassword.py:OnePasswordLoginBackend._item_selector
 * `op item get` target for a listed login: {"args":[...]} or {"error":"..."} when the
 * handle is not a listed Login item even after a refresh. */
json_t *w19_onepasswor_6beff4_item_selector(const char *prefix, const char *handle,
                                           json_t *item_vaults, json_t *listed_vaults);

/* PoP: w19_onepasswor_6beff4_env @ agent/vault_backends/onepassword.py:OnePasswordLoginBackend._env
 * The op child environment: the launch allowlist minus OP_CONNECT_*, the profile's
 * Connect secrets, NO_COLOR, OP_ACCOUNT and either the service token or the session. */
json_t *w19_onepasswor_6beff4_env(json_t *cfg, const char *service_token,
                                 const char *session_token, json_t *env_map, json_t *secrets);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ONEPASSWOR_6BEFF4_H */
