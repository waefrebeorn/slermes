/**
 * port_runtime_provider.c — Port of Python: cli.py (runtime provider helpers)
 *
 * Real C implementations for runtime provider functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: getenv */
char *getenv_fn(const char *name, const char *default_val)
{
    if (!name) {
        hermes_log(LOG_WARNING, "port", "getenv_fn: null name");
        return default_val ? strdup(default_val) : NULL;
    }
    const char *val = getenv(name);
    if (val) {
        hermes_log(LOG_DEBUG, "port", "getenv_fn: %s=%s", name, val);
        return strdup(val);
    }
    hermes_log(LOG_DEBUG, "port", "getenv_fn: %s not set, using default", name);
    return default_val ? strdup(default_val) : NULL;
}

/* Port of Python: _getenv */
char *_getenv(void *ctx, void *name, void *default_val)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_getenv: null context");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "port", "_getenv: called");
    if (name) {
        hermes_log(LOG_DEBUG, "port", "_getenv: name is set");
    }
    if (default_val) {
        hermes_log(LOG_DEBUG, "port", "_getenv: default_val is set");
    }
    return getenv_fn((const char *)name, (const char *)default_val);
}

/* Port of Python: canonical_custom_identity */
void canonical_custom_identity(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "canonical_custom_identity: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "canonical_custom_identity: called");
    const char *identity = getenv("HERMES_CUSTOM_IDENTITY");
    if (identity) {
        hermes_log(LOG_INFO, "port", "canonical_custom_identity: %s", identity);
    } else {
        hermes_log(LOG_DEBUG, "port", "canonical_custom_identity: no custom identity");
    }
}
