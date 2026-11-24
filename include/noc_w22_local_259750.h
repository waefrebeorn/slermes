#ifndef NOC_W22_LOCAL_259750_H
#define NOC_W22_LOCAL_259750_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/vault_backends/local.py (gate w22_local_259750).
 *
 * The local Fernet vault is the always-on default login backend: it lists
 * metadata (never secrets) and resolves one item's secret at fill time. The
 * decrypted store walk plus its key is the DB seam, so the store's rows arrive
 * as the `records` array the caller already decrypted:
 *
 *   [{"id":"...","kind":"login","label":"...","origin":"https://...",
 *     "created_at":"...","identifier":"me@example.com",
 *     "identifier_type":"email","secret":{"password":"...","otp_secret":"..."}}]
 *
 * Everything the backend does with those rows is computed here: the
 * VaultItemMeta rendering, the str() coercion of every secret value, the
 * `or ""` fallbacks, and the RFC 6238 code resolve_otp mints from a stored
 * seed (the wall clock is injected through `at_epoch`).
 *
 * Every answer is a wrapper so a VaultError is data and not a crash:
 *   {"ok": true,  "value": <result>}
 *   {"ok": false, "error": "no vault item with id '<handle>'" }
 */

/* PoP: w22_local_259750_backend_info @ agent/vault_backends/local.py:LocalLoginBackend
 * The class attributes `backend_for_handle` keys on:
 * {"name":"local","display_name":"Hermes vault","prefix":"vault_",
 *  "needs_unlock":false}. */
json_t *w22_local_259750_backend_info(void);

/* PoP: w22_local_259750_list_items @ agent/vault_backends/local.py:LocalLoginBackend.list_items
 * `[VaultStore._meta(rec).to_dict() for rec in records]`, in store order.
 * to_dict emits id/kind/label/origin/created_at always, identifier +
 * identifier_type only when an identifier is set, and has_otp only when the
 * secret carries a non-empty otp_secret. allowed_origins is empty on this
 * backend, so it never appears. */
json_t *w22_local_259750_list_items(const json_t *records);

/* PoP: w22_local_259750_get_meta @ agent/vault_backends/local.py:LocalLoginBackend.get_meta
 * The first record whose `id` equals the handle, rendered as to_dict(), or
 * null when no row matches (get_meta returns None -- it does not raise). */
json_t *w22_local_259750_get_meta(const json_t *records, const char *handle);

/* PoP: w22_local_259750_resolve_password @ agent/vault_backends/local.py:LocalLoginBackend.resolve_password
 * `str(resolve_secret(handle).get("password") or "")`: a falsy password (absent,
 * null, "", 0, false) reads as "", anything else through str(). */
json_t *w22_local_259750_resolve_password(const json_t *records, const char *handle);

/* PoP: w22_local_259750_resolve_secret @ agent/vault_backends/local.py:LocalLoginBackend.resolve_secret
 * `{k: str(v) for k, v in secret.items()}` -- every key of the stored payload,
 * in store order, each value stringified (None -> "None"). */
json_t *w22_local_259750_resolve_secret(const json_t *records, const char *handle);

/* PoP: w22_local_259750_resolve_otp @ agent/vault_backends/local.py:LocalLoginBackend.resolve_otp
 * `seed = str(secret.get("otp_secret") or ""); totp_now(seed) if seed else
 * None`. A seed may carry the `SEED|digits|period|algo` form
 * (agent/vault_store.normalize_otp_secret); algo is SHA1 (default), SHA256 or
 * SHA512, case-insensitive. `at_epoch` is the clock seam as a decimal string
 * (NULL reads as 0). 6-digit codes are returned zero-padded. */
json_t *w22_local_259750_resolve_otp(const json_t *records, const char *handle,
                                     const char *at_epoch);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_LOCAL_259750_H */
