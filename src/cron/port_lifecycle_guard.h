#ifndef CRON_LIFECYCLE_GUARD_H
#define CRON_LIFECYCLE_GUARD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: contains_gateway_lifecycle_command @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command */
bool cron_lifecycle_contains_gateway_lifecycle_command(const char *text);

/* PoP: _resolve_script_path @ cron/lifecycle_guard.py:_resolve_script_path */
/* Resolve a cron `script` value the way the scheduler does: absolute paths
 * pass through; bare/relative paths live under <SLERMES_HOME>/scripts/.
 * Returns a malloc'd string (caller frees). */
char *cron_lifecycle_resolve_script_path(const char *script_path);

/* PoP: _read_script_for_scanning @ cron/lifecycle_guard.py:_read_script_for_scanning */
/* Read a cron script with the bounded terminal-script scanner. Non-regular or
 * oversized inputs fail closed by returning the lifecycle-shaped sentinel
 * "hermes gateway restart"; missing/unreadable paths return "".
 * Returns a malloc'd string (caller frees). */
char *cron_lifecycle_read_script_for_scanning(const char *script_path);

/* PoP: check_gateway_lifecycle @ cron/lifecycle_guard.py:check_gateway_lifecycle */
/* Returns a malloc'd block message when prompt/script contains a gateway
 * lifecycle command or persistent launchctl submit (else NULL). */
char *cron_lifecycle_check_gateway_lifecycle(const char *prompt,
                                             const char *script);

/* PoP: _iter_command_segments @ cron/lifecycle_guard.py:_iter_command_segments */
/* Shell-tokenize `command` into segments (honoring quotes, comments, and
 * ;&|() punctuation boundaries). Returns a NULL-terminated array of
 * NULL-terminated token arrays; caller frees with
 * cron_lifecycle_free_segments. */
char ***cron_lifecycle_iter_command_segments(const char *command);
void cron_lifecycle_free_segments(char ***segments);

/* PoP: _command_token_index @ cron/lifecycle_guard.py:_command_token_index */
/* Index of the executable token after simple env assignments (FOO=bar),
 * or -1 when every token is an assignment. */
int cron_lifecycle_command_token_index(char *const *segment);

/* PoP: contains_launchctl_submit_command @ cron/lifecycle_guard.py:contains_launchctl_submit_command */
bool cron_lifecycle_contains_launchctl_submit_command(const char *command);

/* PoP: _resolve_terminal_script_path @ cron/lifecycle_guard.py:_resolve_terminal_script_path */
char *cron_lifecycle_resolve_terminal_script_path(const char *candidate,
                                                  const char *cwd);

/* PoP: _iter_referenced_shell_scripts @ cron/lifecycle_guard.py:_iter_referenced_shell_scripts */
/* Scripts executed directly or through a POSIX shell. Returns a NULL-terminated
 * array of malloc'd path strings (caller frees each + the array). */
char **cron_lifecycle_iter_referenced_shell_scripts(const char *command,
                                                    const char *cwd);

/* PoP: _iter_shell_command_payloads @ cron/lifecycle_guard.py:_iter_shell_command_payloads */
char **cron_lifecycle_iter_shell_command_payloads(const char *command);

/* PoP: _resolve_script_directory @ cron/lifecycle_guard.py:_resolve_script_directory */
char *cron_lifecycle_resolve_script_directory(const char *script_path);

/* PoP: _read_referenced_script @ cron/lifecycle_guard.py:_read_referenced_script */
/* (text, unsafe) bounded regular-file-only read. Returns malloc'd text or
 * NULL; *out_unsafe set when the file is non-regular or oversized. */
char *cron_lifecycle_read_referenced_script(const char *path, bool *out_unsafe);

/* Remote read callback for contains_..._or_referenced_script (may be NULL). */
typedef char *(*cron_lifecycle_read_remote_fn)(const char *path, void *ctx);

/* PoP: contains_gateway_lifecycle_command_or_referenced_script @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command_or_referenced_script */
bool cron_lifecycle_contains_gateway_lifecycle_command_or_referenced_script(
    const char *command, const char *cwd,
    cron_lifecycle_read_remote_fn read_remote_script, void *read_ctx);

#ifdef __cplusplus
}
#endif

#endif /* CRON_LIFECYCLE_GUARD_H */
