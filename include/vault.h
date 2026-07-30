/*
 * vault.h — Encrypted credential vault accessor.
 *
 * Faithful extraction from the monolithic hermes.h god header (the
 * god-header-elimination pass). The vault lives in src/agent/vault.c.
 * Translation units that read secrets via the vault no longer drag in
 * the entire master header.
 */

#ifndef VAULT_H
#define VAULT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Retrieve a credential value from the encrypted vault. Returns a
 * heap-allocated string (caller frees) or NULL if not found. */
const char *vault_retrieve(const char *service, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* VAULT_H */
