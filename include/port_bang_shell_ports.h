#ifndef PORT_BANG_SHELL_PORTS_H
#define PORT_BANG_SHELL_PORTS_H

#include <stdbool.h>

/* Port of hermes_cli/bang_shell.py — !<command> shell mode for the CLI */

/* is_bang_command: return True when text starts with '!' */
bool bang_shell_is_bang_command(const char *text);

/* parse_bang_command: return the command after '!'. Caller must free. */
char *bang_shell_parse_bang_command(const char *text);

/* bang_shell_enabled: True only for interactive local CLI sessions */
bool bang_shell_enabled(void);

/* resolve_bang_cwd: return cwd for bang command, or NULL to inherit. */
const char *bang_shell_resolve_cwd(const char *session_key);

/* check_bang_approval: run command through safety gate.
 * Returns malloc'd JSON: {"approved": bool, "message": "..."}.
 * Caller must free. */
char *bang_shell_check_approval(const char *command);

/* _bang_env: filtered environment as JSON object string. Caller must free. */
char *bang_shell_bang_env(void);

/* run_bang_command: execute command via shell, stream output.
 * writer_ctx: optional callback (void(*)(const char*)) or NULL for printf.
 * Returns exit code. */
int bang_shell_run_command(const char *command, const char *cwd,
                            int timeout_seconds, void *writer_ctx);

#endif /* PORT_BANG_SHELL_PORTS_H */
