/* update_cmd_more.h — additional C11 ports from hermes_cli/update_cmd.py */

#ifndef HERMES_UPDATE_CMD_MORE_H
#define HERMES_UPDATE_CMD_MORE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── hermes_cli/update_cmd.py additional ports ─────────────────────── */

/* PoP: _shim_quarantine_error_type @ hermes_cli/update_cmd.py:_shim_quarantine_error_type
 * The strict-quarantine refusal type, resolved lazily. */
bool uc_shim_quarantine_error_type_exists(void);

/* PoP: _should_zip_fallback_on_update_error @ hermes_cli/update_cmd.py:_should_zip_fallback_on_update_error
 * ZIP fallback is for Windows git file-I/O breakage, not later stages. */
bool uc_should_zip_fallback_on_update_error(int exit_code, bool is_git_error, bool is_windows);

/* PoP: _read_project_version @ hermes_cli/update_cmd.py:_read_project_version
 * Read the version field from the checkout's pyproject.toml. */
char *uc_read_project_version(const char *project_root);

/* PoP: _classify_fetch_failure @ hermes_cli/update_cmd.py:_classify_fetch_failure
 * Map git-fetch stderr to a one-line, user-facing diagnosis. */
char *uc_classify_fetch_failure(const char *stderr_text);

/* PoP: _print_fetch_failure @ hermes_cli/update_cmd.py:_print_fetch_failure
 * Print the classified diagnosis plus the first raw stderr line. */
void uc_print_fetch_failure(const char *stderr_text);

/* PoP: _purge_stale_hermes_modules @ hermes_cli/update_cmd.py:_purge_stale_hermes_modules
 * Evict every cached Hermes module after the checkout changed in-place. */
void uc_purge_stale_hermes_modules(void);

/* PoP: _reload_process_scan_modules @ hermes_cli/update_cmd.py:_reload_process_scan_modules
 * Force-reload the process-scan modules from disk after an update. */
void uc_reload_process_scan_modules(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_UPDATE_CMD_MORE_H */
