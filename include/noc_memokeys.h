#ifndef NOC_MEMOKEYS_H
#define NOC_MEMOKEYS_H
#include <stddef.h>

/* PoP: memokeys_fingerprint @ agent/credential_persistence.py:fingerprint_secret_value
 * NULL for NULL/empty input; else "sha256:<first 16 hex>". Caller frees. */
char *memokeys_fingerprint(const char *value);

/* PoP: memokeys_endpoint_memo_key @ agent/model_metadata_memo_keys.py:endpoint_memo_key
 * api_key_is_null != 0 models Python None. Caller frees both out strings. */
void memokeys_endpoint_memo_key(const char *normalized, const char *api_key,
                                int api_key_is_null, char **out_norm, char **out_slot);

/* PoP: memokeys_endpoint_disk_slot @ agent/model_metadata_memo_keys.py:endpoint_disk_slot
 * NULL/empty slot models a falsy Python slot. Caller frees. */
char *memokeys_endpoint_disk_slot(const char *normalized, const char *slot);

/* PoP: memokeys_minted_slot @ agent/model_metadata_memo_keys.py:endpoint_memo_key
 * Renders f"minted:{id:x}" for an explicit 64-bit id (decimal string at the gate).
 * Caller frees. */
char *memokeys_minted_slot(const char *id_decimal);

#endif /* NOC_MEMOKEYS_H */
