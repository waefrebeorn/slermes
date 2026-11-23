/*
 * env_settings.c — C port of upstream Hermes cron/env_settings.py.
 * PoP: cronenv_cron_env_setting @ cron/env_settings.py:cron_env_setting
 */
#include "noc_cronenv.h"
#include <stdlib.h>
#include <string.h>

/*
 * Python:
 *   if not is_multiplex_active(): return os.getenv(name) or default
 *   scope = current_secret_scope()
 *   if scope is None: scope = build_profile_secret_scope(get_hermes_home())
 *   value = scope.get(name)
 *   return default if value is None else str(value)
 *
 * The multiplex / scope selection is supplied by the caller (harness reads it
 * from the fixture). env_val is the os.environ value; scope_val is the value
 * from current_secret_scope(); home_scope_val is the value from the scope built
 * for get_hermes_home() when no current scope exists. Numeric/bool scope values
 * arrive as their Python str() form (decimal / "True" / "False").
 */
char *cronenv_cron_env_setting(const char *name, const char *default_val,
                               int multiplex, int has_scope,
                               const char *env_val,
                               const char *scope_val, const char *home_scope_val) {
    (void)name; /* the caller has already selected the value for this name */
    const char *d = default_val ? default_val : "";
    const char *result;
    if (!multiplex) {
        /* `os.getenv(name) or default` — the empty string is falsy in Python. */
        result = (env_val && env_val[0]) ? env_val : d;
    } else {
        const char *value = has_scope ? scope_val : home_scope_val;
        result = value ? value : d; /* `None` -> default, else str(value) */
    }
    return strdup(result ? result : "");
}
