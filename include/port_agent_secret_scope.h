/* port_agent_secret_scope.h — profile-scoped credential resolution.
 *
 * Port of Python agent/secret_scope.py. Thread-local secret scope for
 * multi-profile gateway multiplexing: a turn installs its profile's .env
 * secrets, readers resolve through the scope, and reset restores the
 * previous scope.
 *
 * Ownership: secret_scope_set_secret_scope returns the PREVIOUS scope as
 * an opaque token; pass it back to secret_scope_reset_secret_scope, which
 * frees the interim scope and re-installs the token. The token is owned by
 * the caller between set and reset. secret_scope_current_secret_scope and
 * secret_scope_get_secret return BORROWED pointers (thread-local scope /
 * process env) — never free them. secret_scope_load_env_file and
 * secret_scope_build_profile_secret_scope return a fresh json_t object
 * (caller frees with json_free); NULL only on allocation failure.
 */
#ifndef PORT_AGENT_SECRET_SCOPE_H
#define PORT_AGENT_SECRET_SCOPE_H

#include <stdbool.h>
#include "hermes_json.h"

/* Opaque reset token: the previous thread-local scope (may be NULL). */
typedef json_t *secret_scope_token_t;

/* Process-global multiplex flag. When active, get_secret refuses to fall
 * back to os.environ for non-global names without an installed scope. */
bool secret_scope_set_multiplex_active(bool active);
bool secret_scope_is_multiplex_active(void);

/* Install a scope parsed from secrets_json (NULL/empty -> empty object).
 * Returns the previous scope token for reset. */
secret_scope_token_t secret_scope_set_secret_scope(const char *secrets_json);
void secret_scope_reset_secret_scope(secret_scope_token_t token);

/* Borrowed views of the current thread's scope. */
json_t *secret_scope_current_secret_scope(void);
const char *secret_scope_get_secret(const char *name, const char *default_val);

/* True for process-global (non-profile-secret) env names, matching the
 * live _GLOBAL_ENV_EXACT / _GLOBAL_ENV_PREFIXES allowlist. */
bool secret_scope_is_global_env_fn(const char *name);

/* Parse a .env file / build a profile scope from <hermes_home>/.env.
 * Fresh json_t object, caller frees. Missing file -> empty object. */
json_t *secret_scope_load_env_file(const char *path);
json_t *secret_scope_build_profile_secret_scope(const char *hermes_home);

#endif /* PORT_AGENT_SECRET_SCOPE_H */
