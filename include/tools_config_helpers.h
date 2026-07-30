/*
 * tools_config_helpers.h — pure helper ports of hermes_cli/tools_config.py
 *
 * Faithful C ports of the *pure* display/config helpers in tools_config.py:
 *
 *   - gui_toolset_label(label)        — strip a leading non-alphanumeric
 *                                        icon/emoji token from a toolset title
 *   - toolset_allowed_for_platform()  — platform-scoped toolset gating
 *   - parse_enabled_flag(value,def)   — bool-like config value parsing
 *   - format_imagegen_model_row(...)  — column-aligned model picker row
 *
 * These are deterministic and side-effect-free. The platform-restriction
 * table is embedded as constant data (it is a fixed module-level dict in the
 * Python source). No network / filesystem / tiktoken dependencies.
 *
 * Opaque where needed, minimal includes, C11.
 */

#ifndef HERMES_TOOLS_CONFIG_HELPERS_H
#define HERMES_TOOLS_CONFIG_HELPERS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Strip a leading icon/emoji token (e.g. "🔌 Plugins" -> "Plugins").
 * Returns a malloc'd string the caller frees. */
char *gui_toolset_label(const char *label);

/* Return true if ts_key is configurable on platform. Toolsets absent from the
 * restriction table are allowed everywhere. */
bool toolset_allowed_for_platform(const char *ts_key, const char *platform);

/* Parse a bool-like config value:
 *   NULL / missing -> def
 *   bool -> as-is
 *   int  -> != 0
 *   str  -> true/1/yes/on  vs  false/0/no/off  (case-insensitive, trimmed)
 *   anything else -> def */
bool parse_enabled_flag(const char *value, bool default_val);

/* Format a single imagegen model picker row, column-aligned.
 * `widths` provides the column widths (model/speed/strengths). Returns a
 * malloc'd string the caller frees. Mirrors the Python f-string layout. */
char *format_imagegen_model_row(const char *model_id,
                                 const char *speed,
                                 const char *strengths,
                                 const char *price,
                                 int width_model,
                                 int width_speed,
                                 int width_strengths);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOLS_CONFIG_HELPERS_H */
