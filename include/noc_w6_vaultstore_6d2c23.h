#ifndef NOC_W6_VAULTSTORE_6D2C23_H
#define NOC_W6_VAULTSTORE_6D2C23_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_vaultstore_6d2c23_vault_kinds @ agent/vault_store.py:VAULT_KINDS
 * Returns a NEW JSON array of the three canonical item kinds. Caller frees. */
struct json_t *w6_vaultstore_6d2c23_vault_kinds(void);

/* PoP: w6_vaultstore_6d2c23_login_identifier_types @ agent/vault_store.py:LOGIN_IDENTIFIER_TYPES
 * Returns a NEW JSON array of the accepted login identifier types. Caller frees. */
struct json_t *w6_vaultstore_6d2c23_login_identifier_types(void);

/* PoP: w6_vaultstore_6d2c23_fields @ agent/vault_store.py:PAYMENT_FIELDS / ADDRESS_FIELDS
 * "payment"/"address" select the canonical field->autocomplete map; anything
 * else yields an empty object. Caller frees. */
struct json_t *w6_vaultstore_6d2c23_fields(const char *kind);

/* PoP: w6_vaultstore_6d2c23_required_fields @ agent/vault_store.py:REQUIRED_FIELDS
 * "payment"/"address" select the required field tuple; anything else yields
 * an empty array. Caller frees. */
struct json_t *w6_vaultstore_6d2c23_required_fields(const char *kind);

/* PoP: w6_vaultstore_6d2c23_normalize_origin @ agent/vault_store.py:normalize_origin
 * scheme://host[:port] with default ports stripped.  Returns a malloc'd string,
 * or NULL where Python raises VaultError (caller maps that to None). Caller frees. */
char *w6_vaultstore_6d2c23_normalize_origin(const char *url_or_origin);

/* PoP: w6_vaultstore_6d2c23_normalize_otp_secret @ agent/vault_store.py:normalize_otp_secret
 * Plain base32 seed or otpauth://totp URI.  Returns a malloc'd canonical seed
 * ("seed|digits|period|algo" for non-defaults), or NULL where Python raises.
 * Caller frees. */
char *w6_vaultstore_6d2c23_normalize_otp_secret(const char *value);

/* PoP: w6_vaultstore_6d2c23_scrub_secret_from_text @ agent/vault_store.py:scrub_secret_from_text
 * Replaces secret string values (len >= 3) and password key=value echoes.
 * `secret` is a JSON object; returns a malloc'd string (caller frees). */
char *w6_vaultstore_6d2c23_scrub_secret_from_text(const char *text,
                                                  const struct json_t *secret);

/* PoP: w6_vaultstore_6d2c23_meta_dict @ agent/vault_store.py:VaultStore._meta + VaultItemMeta.to_dict
 * Metadata-only projection of a stored record (never includes the secret
 * payload). Returns a NEW JSON object. Caller frees. */
struct json_t *w6_vaultstore_6d2c23_meta_dict(const struct json_t *rec);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_VAULTSTORE_6D2C23_H */
