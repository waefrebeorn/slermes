/*
 * port_gateway_windows.h -- pure helpers ported from
 * hermes_cli/gateway_windows.py.
 *
 * The SCM / schtasks / UAC / registry half stays in Python.
 */
#ifndef PORT_GATEWAY_WINDOWS_H
#define PORT_GATEWAY_WINDOWS_H

#include <stddef.h>

/* _normalize_windows_path -- strip, lstrip('"'), '\'->'/', rstrip('/'),
 * casefold. Caller frees. */
char *gw_normalize_path(const char *value);

/* _sanitize_filename -- replace the Windows-illegal set with '_'. Caller frees. */
char *gw_sanitize_filename(const char *value);

/* _quote_vbs_string -- a VBScript string literal (embedded quotes doubled).
 * Refuses a newline: returns NULL and sets *err. Caller frees both. */
char *gw_quote_vbs(const char *value, char **err);

/* _is_access_denied -- the 7-locale access-denied pattern. */
int gw_is_access_denied(const char *detail);

/* _should_fall_back -- exit 124, or a fallback-pattern match. */
int gw_should_fall_back(int code, const char *detail);

/* _gateway_run_argv -- python -m hermes_cli.main [profile...] gateway run.
 * `profile_arg` is a pre-split NULL-terminated list. Caller frees each string
 * and the array. */
char **gw_run_argv(const char *python_exe, const char *const *profile_arg,
                   int *out_count);

/* hermes_owns_windows_service -- a Hermes-named service, or one whose binary
 * sits under a Hermes root. */
int gw_owns_service(const char *name, const char *binpath,
                    const char *const *hermes_roots);

#endif /* PORT_GATEWAY_WINDOWS_H */
