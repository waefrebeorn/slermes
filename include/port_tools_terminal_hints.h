/*
 * port_tools_terminal_hints.h — C11 port of tools/terminal_hints.py
 */
#ifndef PORT_TOOLS_TERMINAL_HINTS_H
#define PORT_TOOLS_TERMINAL_HINTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Output-pattern failure hints for the terminal tool.
 * Each returns a malloc'd hint string (caller frees) or NULL when the
 * pattern does not match. */
char *thint_hint_gh_unknown_json_field(const char *command, const char *output);
char *thint_hint_command_not_found(const char *command, const char *output);
char *thint_hint_module_not_found(const char *command, const char *output);
char *thint_hint_merge_conflict(const char *command, const char *output);
char *thint_hint_already_exists(const char *command, const char *output);
char *thint_hint_gh_rate_limit(const char *command, const char *output);
char *thint_hint_permission_denied(const char *command, const char *output);

/* PoP: annotate_failure @ tools/terminal_hints.py:annotate_failure */
/* Return one short recovery hint for a failed command, or NULL.
 * Returns NULL for exit_code == 0. Scans only the first 4000 chars of
 * output. Caller frees the result. */
char *thint_annotate_failure(const char *command, int exit_code, const char *output);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_TERMINAL_HINTS_H */
