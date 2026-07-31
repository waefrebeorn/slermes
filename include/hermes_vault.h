/* Self-contained public API. No god headers — opaque types via core_types only.
 * C11 only.
 */
#ifndef SLERMES_VAULT_H
#define SLERMES_VAULT_H

#include "hermes_core_types.h"

/* Credential vault */
bool vault_set_master_key(const char *passphrase);
bool vault_has_master_key(void);
void vault_lock(void);
void vault_set_path(const char *path);
bool vault_save(void);
bool vault_load(void);
bool vault_store(const char *service, const char *key, const char *value);
const char *vault_retrieve(const char *service, const char *key);
bool vault_delete(const char *service, const char *key);
int  vault_list_services(char services[][128], int max_count);
bool vault_rotate_key(const char *old_passphrase, const char *new_passphrase);

#endif /* SLERMES_VAULT_H */
