/*
 * config_defaults.h — PoP of hermes_cli/config_defaults.py (pure-data leaf).
 *
 * Python names -> C symbols (1:1):
 *   DEFAULT_CONFIG    -> config_defaults_DEFAULT_CONFIG()      (fresh json_t*)
 *   OPTIONAL_ENV_VARS -> config_defaults_OPTIONAL_ENV_VARS()   (fresh json_t*)
 * plus the verbatim canonical JSON text accessors used by the oracle.
 * Callers own the returned tree and release it with json_free().
 */
#ifndef HERMES_CLI_CONFIG_DEFAULTS_H
#define HERMES_CLI_CONFIG_DEFAULTS_H

#include <stdint.h>

/* forward decl — real definition in lib/libjson/json.h */
typedef struct json_t json_t;

json_t     *config_defaults_DEFAULT_CONFIG(void);
const char *config_defaults_DEFAULT_CONFIG_json(void);
json_t     *config_defaults_OPTIONAL_ENV_VARS(void);
const char *config_defaults_OPTIONAL_ENV_VARS_json(void);

#endif /* HERMES_CLI_CONFIG_DEFAULTS_H */
