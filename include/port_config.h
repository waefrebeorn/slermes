/* port_config.h -- pure helpers ported from hermes_cli/config.py. */
#ifndef SLERMES_PORT_CONFIG_H
#define SLERMES_PORT_CONFIG_H

#include <stddef.h>

#include "libjson/json.h"

// split_key_path: split a dotted key, honouring backslash-escaped dots.
// Writes up to `max` NUL-terminated segments into `out`; returns the segment
// count, or -1 on overflow / bad args.
int cfg_split_key_path(const char *key, char **out, int max);

// env_var_policy_name: upper-case on Windows, verbatim elsewhere.
void cfg_env_var_policy_name(const char *key, int is_windows,
                             char *out, size_t cap);

// _is_env_config_key: credential-shaped keys route to .env.
int cfg_is_env_config_key(const char *key);

// validate_env_var_name_for_write outcomes.
enum { CFG_ENV_OK = 0, CFG_ENV_INVALID_NAME = 1, CFG_ENV_DENIED = 2 };
int cfg_validate_env_var_name(const char *key, int is_windows);

/* install-method helpers (detect_install_method itself reads the filesystem) */
int cfg_is_nix_install_method(const char *method);
void cfg_recommended_update_command_for_method(const char *method,
                                               char *out, size_t cap);
int cfg_is_uv_tool_install(void);
int cfg_is_unsupported_install_method(const char *method);
void cfg_format_unsupported_install_warning(const char *method,
                                            char *out, size_t cap);

/* config-structure validation. `issues` is a JSON array; each entry appended
 * is {severity, message, hint}, mirroring ConfigIssue. */
void ac_require_fields(json_t *issues, json_t *entry, const char *label,
                       const char **fields, int nfields, const char *suffix);
void ac_validate_entry_list(json_t *issues, json_t *entries, const char *label,
                            const char **fields, int nfields,
                            const char *severity, const char *message,
                            const char *hint);
void ac_validate_custom_providers(json_t *issues, json_t *cp);
void ac_validate_fallback_model(json_t *issues, json_t *fb);

#endif /* SLERMES_PORT_CONFIG_H */
