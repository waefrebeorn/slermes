/* tools_config_cua.h — faithful C11 port of the cua-driver install/status
 * surface of hermes_cli/tools_config.py (plus the two cua_backend.py
 * resolution helpers it delegates to).
 *
 * Opaque: no god header. json_t only, via hermes_json.h.
 * Every returned string or string list is malloc'd; free with free() or
 * tcc_free_list().
 */
#ifndef SLERMES_TOOLS_CONFIG_CUA_H
#define SLERMES_TOOLS_CONFIG_CUA_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* ── cua_backend.py resolution helpers (reused by the tools_config surface) ── */

/* PoP: cua_backend_candidate_cua_driver_commands @ tools/computer_use/cua_backend.py:_candidate_cua_driver_commands
 * Candidate commands in resolution order. override (or HERMES_CUA_DRIVER_CMD)
 * is authoritative -> single-element list. Caller tcc_free_list()s. */
char **cua_backend_candidate_cua_driver_commands(const char *override,
                                                 size_t *out_n);

/* PoP: cua_backend_resolve_cua_driver_cmd @ tools/computer_use/cua_backend.py:resolve_cua_driver_cmd
 * First candidate that resolves on PATH (or as an executable path).
 * NULL when none. malloc'd. */
char *cua_backend_resolve_cua_driver_cmd(const char *override);

/* PoP: cua_backend_cua_driver_runtime_contract_status @ tools/computer_use/cua_backend.py:cua_driver_runtime_contract_status
 * {ready,binary,version,reason}. Runs "<binary> manifest" and validates the
 * >=0.20.0 semver + mcp_invocation.args + required subcommand args.
 * Caller json_free()s. */
json_t *cua_backend_cua_driver_runtime_contract_status(const char *binary);

/* ── hermes_cli/tools_config.py cua surface ── */

/* PoP: tools_config_cua_driver_cmd @ hermes_cli/tools_config.py:_cua_driver_cmd */
char *tools_config_cua_driver_cmd(void);

/* PoP: tools_config_resolved_cua_driver_cmd @ hermes_cli/tools_config.py:_resolved_cua_driver_cmd */
char *tools_config_resolved_cua_driver_cmd(void);

/* PoP: tools_config_cua_install_home @ hermes_cli/tools_config.py:_cua_install_home */
char *tools_config_cua_install_home(void);

/* PoP: tools_config_cua_install_lock_dir @ hermes_cli/tools_config.py:_cua_install_lock_dir */
char *tools_config_cua_install_lock_dir(void);

/* PoP: tools_config_cua_windows_install_lock_file @ hermes_cli/tools_config.py:_cua_windows_install_lock_file */
char *tools_config_cua_windows_install_lock_file(void);

/* PoP: tools_config_ps_single_quote @ hermes_cli/tools_config.py:_ps_single_quote */
char *tools_config_ps_single_quote(const char *value);

/* PoP: tools_config_cua_version_summary @ hermes_cli/tools_config.py:_cua_version_summary
 * First non-empty line of raw, truncated to limit (Python default 120). */
char *tools_config_cua_version_summary(const char *raw, size_t limit);

/* PoP: tools_config_cua_install_target_writable @ hermes_cli/tools_config.py:_cua_install_target_writable */
bool tools_config_cua_install_target_writable(void);

/* PoP: tools_config_cua_driver_env @ hermes_cli/tools_config.py:_cua_driver_env
 * NULL-terminated "K=V" env block with the telemetry policy applied. */
char **tools_config_cua_driver_env(size_t *out_n);

/* PoP: tools_config_cua_driver_contract_status @ hermes_cli/tools_config.py:_cua_driver_contract_status
 * 30s fingerprint-cached wrapper over the cua_backend contract status. */
json_t *tools_config_cua_driver_contract_status(const char *binary);

/* PoP: tools_config_cua_driver_autostart_registered_windows @ hermes_cli/tools_config.py:_cua_driver_autostart_registered_windows */
bool tools_config_cua_driver_autostart_registered_windows(void);

/* PoP: tools_config_cua_driver_install_ready @ hermes_cli/tools_config.py:_cua_driver_install_ready */
bool tools_config_cua_driver_install_ready(void);

/* PoP: tools_config_post_setup_already_installed @ hermes_cli/tools_config.py:_post_setup_already_installed
 * Registered predicate lookup (_POST_SETUP_INSTALLED); unregistered -> true. */
bool tools_config_post_setup_already_installed(const char *post_setup_key);

/* PoP: tools_config_post_setup_no_window_flags @ hermes_cli/tools_config.py:_post_setup_no_window_flags
 * Win32 CREATE_NO_WINDOW; 0 on POSIX, and 0 when streams_to_console and the
 * process owns a tty. */
int tools_config_post_setup_no_window_flags(bool streams_to_console);

void tcc_free_list(char **list, size_t n);

#endif /* SLERMES_TOOLS_CONFIG_CUA_H */
