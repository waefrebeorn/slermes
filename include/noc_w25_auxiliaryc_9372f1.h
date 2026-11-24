/*
 * noc_w25_auxiliaryc_9372f1.h - C surface for upstream
 * agent/auxiliary_custom_credentials.py (gate w25_auxiliaryc_9372f1).
 *
 * Credential selection for an explicit custom endpoint: whoever owns the URL
 * owns the key.  The config lookup, the env read and the ``key_cmd`` helper
 * mint are seams; ported here is the precedence itself -- what a declared
 * credential is, what identity a cache may key on, and which provider's
 * catalog supplies the fast model.
 */
#ifndef NOC_W25_AUXILIARYC_9372F1_H
#define NOC_W25_AUXILIARYC_9372F1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The entry keys that declare a credential (order matters for the env probe). */
#define W25_AUXILIARYC_9372F1_DECLARED_KEYS 4

/* raw -> {"callable": bool, "value": <str|null>}.  A callable key_cmd/Entra
 * provider passes through uncalled; a string is stripped; anything else is "". */
json_t *w25_auxiliaryc_9372f1_normalize_api_key(const json_t *raw);

/* request: {"base_url", "provider", "found": bool, "entry": {...}|null,
 *           "scoped_key_env": <str>} ->
 * {"found": bool, "parts": [api_key, scoped key_env, "cmd:<key_cmd>"]}.
 * "parts" is empty when no entry owns the URL or it declares no credential;
 * the NUL join of the live identity is kept as three parts so C need not carry
 * an embedded NUL. */
json_t *w25_auxiliaryc_9372f1_identity(const json_t *request);

/* request: {"runtime": {...}, "main_provider": <any>, "found": bool,
 *           "entry": {...}|null, "owns": bool, "known": [<provider>...]} ->
 * the provider whose catalog supplies the fast model (often main_provider). */
json_t *w25_auxiliaryc_9372f1_fast_provider(const json_t *request);

/* entry -> bool: any of api_key/key_env/api_key_env/key_cmd is non-blank
 * (honouring a provenance ``declares_credential`` verdict first). */
json_t *w25_auxiliaryc_9372f1_declares_credential(const json_t *entry);

/* The entry keys that declare a credential. */
json_t *w25_auxiliaryc_9372f1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
