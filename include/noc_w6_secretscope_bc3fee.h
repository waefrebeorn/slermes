#ifndef NOC_W6_SECRETSCOPE_BC3FEE_H
#define NOC_W6_SECRETSCOPE_BC3FEE_H

/*
 * Public C API for the port of agent/secret_scope.py (gate w6_secretscope_bc3fee).
 * All exported symbols are secretscope_-prefixed. Process/context state that
 * the Python module reads from globals is injected explicitly (no setenv).
 */
#include "hermes_json.h"

/* _strip_inline_comment(value) -> str. */
json_t *secretscope_strip_inline_comment(const char *value);

/* _parse_env_value(raw) -> str. */
json_t *secretscope_parse_env_value(const char *raw);

/* _parse_env_text(text) -> {key: value}. */
json_t *secretscope_parse_env_text(const char *text);

/* get_secret(name, default, env, bound, multiplex_active, serves_routed,
 *            launch_scope, global_names, global_prefixes, launch_only_names).
 * Returns a JSON string, JSON null (default None), or
 * {"error":"UnscopedSecretError"} for the fail-closed raise. */
json_t *secretscope_get_secret(const char *name, const json_t *def,
                               const json_t *env, const json_t *bound,
                               int multiplex_active, int serves_routed,
                               const json_t *launch_scope,
                               const json_t *global_names,
                               const json_t *global_prefixes,
                               const json_t *launch_only_names);

#endif /* NOC_W6_SECRETSCOPE_BC3FEE_H */
