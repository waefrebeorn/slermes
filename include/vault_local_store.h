/*
 * vault_local_store.h — the local login backend, wired to the real vault files.
 *
 * agent/vault_backends/local.py delegates every method to the VaultStore. The C side had the backend
 * helpers (over an in-memory records array) and, separately, the encrypted file -- but nothing joined
 * them, so `local` could not answer a single question about an actual vault.
 *
 * These functions are that join, one per backend method. Results use upstream's shape: a metadata
 * array for the listing, the metadata or NULL for a miss, the resolved value, or {"error": ...} when
 * the vault itself cannot be opened. A missing item is an error for the resolve* methods, exactly as
 * VaultStore.resolve_secret raises.
 */
#ifndef HERMES_VAULT_LOCAL_STORE_H
#define HERMES_VAULT_LOCAL_STORE_H

#include "hermes_json.h"

json_t *vault_local_list(const char *base);
json_t *vault_local_get_meta(const char *base, const char *handle);
json_t *vault_local_resolve_password(const char *base, const char *handle);
json_t *vault_local_resolve_secret(const char *base, const char *handle);
/* at_epoch: NULL means "now"; a string epoch makes the code reproducible under test. */
json_t *vault_local_resolve_otp(const char *base, const char *handle, const char *at_epoch);

/* The always-on backend descriptor, so a caller can enumerate without opening a vault. */
json_t *vault_local_backend_info(void);

#endif /* HERMES_VAULT_LOCAL_STORE_H */
