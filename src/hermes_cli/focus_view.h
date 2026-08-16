/*
 * port_hermes_cli_focus_view.h — Focus view display-only reduced-output mode.
 *
 * C11 port of hermes_cli/focus_view.py.
 * Everything here is display-only — pure formatting and state logic.
 * No I/O, no config file access.
 */

#ifndef PORT_HERMES_CLI_FOCUS_VIEW_H
#define PORT_HERMES_CLI_FOCUS_VIEW_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FOCUS_CONFIG_KEY         "display.focus_view"
#define FOCUS_TOOL_PROGRESS_MODE "off"
#define FOCUS_STATUSBAR_LABEL    "\xe2\x97\x89 focus"  /* "◉ focus" in UTF-8 */
#define FOCUS_USAGE              "Usage: /focus [on|off|status]"

/**
 * Coerce a raw config/attr value into a known tool-progress mode.
 * Python: normalize_tool_progress_mode(mode, default="all")
 * In C: pass NULL/empty for mode => use default.
 *       pass "off"/"all"/"new"/"verbose"/"log" => return that.
 *       pass anything else => return default.
 */
const char *fv_normalize_tool_progress_mode(const char *mode_str,
                                             bool mode_is_bool,
                                             bool mode_bool_val,
                                             const char *default_val);

/**
 * Map a /focus argument onto an action.
 * Python: resolve_focus_arg(arg, current) -> (action, target)
 * Returns: "set",1  "set",0  "status" or "usage" as action, target as bool*
 */
const char *fv_resolve_focus_arg(const char *arg, bool current,
                                  bool *out_target);

/**
 * Return the tool-progress mode that should actually be in force.
 * Python: effective_tool_progress_mode(focus_enabled, configured_mode)
 */
const char *fv_effective_tool_progress_mode(bool focus_enabled,
                                             const char *configured_mode_str,
                                             bool mode_is_bool,
                                             bool mode_bool_val);

/**
 * Would the CLI have committed a scrollback line for this tool call?
 * Python: would_display_tool_line(mode, function_name, last_tool_name)
 */
bool fv_would_display_tool_line(const char *mode_str,
                                 bool mode_is_bool,
                                 bool mode_bool_val,
                                 const char *function_name,
                                 const char *last_tool_name);

/**
 * Dim post-turn recovery line, or NULL when nothing was hidden.
 * Caller must free the returned string.
 * Python: format_hidden_line(count) -> str|None
 */
char *fv_format_hidden_line(int count);

/**
 * Status-bar segment text for focus view (empty when off).
 * Returns a pointer to a static string constant — do NOT free.
 * Python: focus_statusbar_segment(enabled)
 */
const char *fv_focus_statusbar_segment(bool enabled);

/**
 * Human-readable /focus status body (no ANSI).
 * Caller must free.
 * Python: format_focus_status(enabled, configured_mode)
 */
char *fv_format_focus_status(bool enabled, const char *configured_mode_str,
                              bool mode_is_bool, bool mode_bool_val);

/**
 * Confirmation line printed when focus view is switched (no ANSI).
 * Caller must free.
 * Python: format_focus_toggle_message(enabled, configured_mode)
 */
char *fv_format_focus_toggle_message(bool enabled,
                                      const char *configured_mode_str,
                                      bool mode_is_bool,
                                      bool mode_bool_val);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_FOCUS_VIEW_H */