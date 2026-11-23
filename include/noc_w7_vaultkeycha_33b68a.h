#ifndef NOC_W7_VAULTKEYCHA_33B68A_H
#define NOC_W7_VAULTKEYCHA_33B68A_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: vaultkeychain_is_os_keychain @ agent/vault_keychain.py:_is_os_keychain
 * Recursive keyring-backend predicate. A backend is described by a JSON object
 * with optional `backends` (array) plus `module` / `type_name` members. A
 * ChainerBackend writes to its highest-priority child; a leaf is an OS
 * keychain unless its defining module is a non-credential-store backend. */
int vaultkeychain_is_os_keychain(const json_t *backend);

/* PoP: vaultkeychain_backend_label @ agent/vault_keychain.py:backend_label
 * Human-readable keychain name, or the exact `unavailable: ...` message. The
 * injected world carries `import_error`, `backend_error` and the backend
 * description used by os_keyring(). */
json_t *vaultkeychain_backend_label(const json_t *world);

/* PoP: vaultkeychain_new_ref @ agent/vault_keychain.py:new_ref
 * New keychain pointer. `account_hex` is the injected uuid.uuid4().hex. */
json_t *vaultkeychain_new_ref(const char *vault_dir, const char *account_hex);

/* PoP: vaultkeychain_owns @ agent/vault_keychain.py:owns
 * False for a pointer copied/moved from another vault directory. */
int vaultkeychain_owns(const json_t *ref, const char *vault_dir);

#ifdef __cplusplus
}
#endif
#endif
