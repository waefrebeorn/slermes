/*
 * port_update_windows.h -- pure helpers ported from
 * hermes_cli/update_cmd_windows.py and gateway/status.py.
 *
 * The process scan / psutil / updater half stays in Python.
 */
#ifndef PORT_UPDATE_WINDOWS_H
#define PORT_UPDATE_WINDOWS_H

#include <stddef.h>

/* _lower_dir_prefix -- lower-cased path with ONE trailing separator, for prefix
 * matching. `resolved` is the resolve() answer; NULL falls back to `path`
 * (the OSError path). Caller frees. */
char *uw_lower_dir_prefix(const char *path, const char *resolved);

/* _cwd_prefix -- lower-cased cwd with one trailing separator. `cwd` NULL yields
 * a bare separator (the unreadable-cwd path). Caller frees. */
char *uw_cwd_prefix(const char *cwd);

/* _gateway_command_subcommand -- the canonical gateway matcher: the lifecycle
 * subcommand of a gateway command line, or NULL. Caller frees. */
char *uw_gateway_subcommand(const char *command);

/* _is_pausable_gateway -- the gateway subcommand is "run". */
int uw_is_pausable_gateway(const char *cmdline);

/* _hermes_holder_subcommand -- the Hermes subcommand a venv-holder argv runs,
 * or NULL. Token-based; never a substring match. Caller frees. */
char *uw_holder_subcommand(const char *cmdline);

/* _venv_holder_kind -- "gateway" / "backend" / "hermes:<sub>" / "python".
 * Caller frees. */
char *uw_holder_kind(const char *cmdline);

#endif /* PORT_UPDATE_WINDOWS_H */
