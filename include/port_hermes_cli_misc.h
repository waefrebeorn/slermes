#ifndef PORT_HERMES_CLI_MISC_H
#define PORT_HERMES_CLI_MISC_H

#include "hermes_json.h"

/* Thin pure helpers consolidated from three modules:
 *  - hermes_cli/auth_commands.py::_display_source
 *  - agent/delegation_registry.py::configured_backend
 *  - agent/context_compressor_telemetry.py::_safe_int
 * The surrounding modules are CLI/plumbing runtime; each pure core is small. */

/* _display_source: strip a "manual:" prefix; else the source unchanged. */
char *misc_display_source(const char *source);

/* configured_backend: delegation.backend stripped+lowered, else "default". */
char *misc_configured_backend(const char *config_json);

/* _safe_int: best-effort int() — number truncates, int/float str parses,
 * bool ->1/0, else -1 (None). */
long long misc_safe_int(const json_t *value);

#endif /* PORT_HERMES_CLI_MISC_H */
