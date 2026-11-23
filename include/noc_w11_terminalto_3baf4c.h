#ifndef NOC_W11_TERMINALTO_3BAF4C_H
#define NOC_W11_TERMINALTO_3BAF4C_H

/* C API for the tools/terminal_tool_sudo.py port (gate w11_terminalto_3baf4c). */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_terminalto_3baf4c_looks_like_env_assignment @ tools/terminal_tool_sudo.py:_looks_like_env_assignment */
json_t *w11_terminalto_3baf4c_looks_like_env_assignment(const char *token);
/* PoP: w11_terminalto_3baf4c_read_shell_token @ tools/terminal_tool_sudo.py:_read_shell_token
 * Returns [token, end_index]. */
json_t *w11_terminalto_3baf4c_read_shell_token(const char *command, int start);
/* PoP: w11_terminalto_3baf4c_scan_shell @ tools/terminal_tool_sudo.py:_scan_shell
 * Returns an array of [kind, start, end, was_start] tuples. */
json_t *w11_terminalto_3baf4c_scan_shell(const char *command, int background);
/* PoP: w11_terminalto_3baf4c_rewrite_sudo @ tools/terminal_tool_sudo.py:_rewrite_real_sudo_invocations
 * Returns [rewritten_command, sudo_count]. */
json_t *w11_terminalto_3baf4c_rewrite_sudo(const char *command, int askpass);
/* PoP: w11_terminalto_3baf4c_count_sudo @ tools/terminal_tool_sudo.py:_count_real_sudo_invocations */
json_t *w11_terminalto_3baf4c_count_sudo(const char *command);
/* PoP: w11_terminalto_3baf4c_rewrite_compound_background @ tools/terminal_tool_sudo.py:_rewrite_compound_background */
json_t *w11_terminalto_3baf4c_rewrite_compound_background(const char *command);
/* PoP: w11_terminalto_3baf4c_sudo_wrong_password @ tools/terminal_tool_sudo.py:_sudo_wrong_password_failure */
json_t *w11_terminalto_3baf4c_sudo_wrong_password(const char *output);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TERMINALTO_3BAF4C_H */
