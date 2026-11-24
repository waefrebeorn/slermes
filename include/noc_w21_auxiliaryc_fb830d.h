/*
 * noc_w21_auxiliaryc_fb830d.h -- the auth_type dispatch table of the auxiliary
 * registry branch (port of agent/auxiliary_client_registry.py).
 *
 * PoP: w21_auxiliaryc_fb830d_registry_arms @ agent/auxiliary_client_registry.py:REGISTRY_AUTHTYPE_ARMS
 * PoP: w21_auxiliaryc_fb830d_arm_for @ agent/auxiliary_client_registry.py:REGISTRY_AUTHTYPE_ARMS
 * PoP: w21_auxiliaryc_fb830d_log_once @ agent/auxiliary_client_registry.py:_log_once_debug
 * PoP: w21_auxiliaryc_fb830d_minimax_resolve @ agent/auxiliary_client_registry.py:_resolve_minimax_oauth_arm
 *
 * The arms late-import the facade, the credential store and the SDK; those are
 * the config/network seams. The port keeps what the module itself decides:
 * which auth_type maps to which arm, the debug-once suppression, and the
 * minimax-oauth resolution ladder (probe mode reads raw state and never
 * refreshes, the non-probe path fails closed to (None, None), the base URL is
 * strip().rstrip("/"), and the model falls back model -> aux model -> M3).
 */
#ifndef NOC_W21_AUXILIARYC_FB830D_H
#define NOC_W21_AUXILIARYC_FB830D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* REGISTRY_AUTHTYPE_ARMS: {auth_type: arm function name}. */
json_t *w21_auxiliaryc_fb830d_registry_arms(void);

/* The arm registered for *auth_type*, or null (the facade's own branches --
 * api_key / external_process -- are deliberately not in this table). */
json_t *w21_auxiliaryc_fb830d_arm_for(const char *auth_type);

/* _log_once_debug over a sequence of keys sharing one seen-set:
 * -> {"logged": [bool...], "seen": [key...]} (seen sorted) */
json_t *w21_auxiliaryc_fb830d_log_once(const json_t *keys, const json_t *seen);

/* _resolve_minimax_oauth_arm as a decision, with every seam injected:
 *   {"probe": bool, "state": {...}|null, "req_model": ..., "aux_model": ...,
 *    "provider": "...", "creds": {"api_key_callable": bool, "api_key": ...,
 *    "base_url": ...}|null, "creds_error": null|"AuthError"|"<Exception text>",
 *    "build_import_error": bool, "seen": [...]}
 * -> {"status":"absent"|"ok", "reason":..., "base_url":..., "model":...,
 *     "kind":"probe"|"anthropic", "debug": msg|null, "logged": bool} */
json_t *w21_auxiliaryc_fb830d_minimax_resolve(const json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_AUXILIARYC_FB830D_H */
