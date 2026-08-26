/* tools_config_install.h — faithful C11 port of the install/post-setup
 * surface of hermes_cli/tools_config.py:
 *   _pip_install, install_cua_driver, _clear_stale_windows_cua_install_lock,
 *   _clear_stale_cua_install_lock, _repair_cua_driver_autostart_windows,
 *   _run_cua_driver_installer, _ensure_browser_use_cli, _run_post_setup.
 *
 * Self-contained subprocess helpers; reuses tools_config_cua.h for the
 * cua-driver resolution/env surface.
 */
#ifndef SLERMES_TOOLS_CONFIG_INSTALL_H
#define SLERMES_TOOLS_CONFIG_INSTALL_H

#include <stdbool.h>
#include <stddef.h>

/* subprocess.CompletedProcess analog. */
typedef struct {
    int returncode;      /* negative => spawn failure / internal error */
    char *out;           /* captured stdout ("" when not captured) */
    char *err;           /* captured stderr ("" when not captured) */
} tools_config_proc_result_t;

/* PoP: tools_config_pip_install @ hermes_cli/tools_config.py:_pip_install
 * uv-first pip install cascade (uv pip install -> python -m pip ->
 * ensurepip bootstrap retry). args are the package arguments only.
 * Returns 0 and fills *out on completion (mirrors returning the
 * CompletedProcess); nonzero when every tier failed to spawn. */
int tools_config_pip_install(char *const *args, size_t n_args,
                             int timeout_s, bool capture_output,
                             tools_config_proc_result_t *out);

/* PoP: tools_config_install_cua_driver @ hermes_cli/tools_config.py:install_cua_driver
 * Install or refresh the cua-driver binary. See the Python docstring for the
 * upgrade / require_confirmed_update / show_installer_progress contract. */
bool tools_config_install_cua_driver(bool upgrade, bool require_confirmed_update,
                                     bool show_installer_progress);

/* PoP: tools_config_clear_stale_windows_cua_install_lock @ hermes_cli/tools_config.py:_clear_stale_windows_cua_install_lock */
void tools_config_clear_stale_windows_cua_install_lock(void);

/* PoP: tools_config_clear_stale_cua_install_lock @ hermes_cli/tools_config.py:_clear_stale_cua_install_lock */
void tools_config_clear_stale_cua_install_lock(void);

/* PoP: tools_config_repair_cua_driver_autostart_windows @ hermes_cli/tools_config.py:_repair_cua_driver_autostart_windows */
bool tools_config_repair_cua_driver_autostart_windows(const char *driver_cmd,
                                                      bool verbose);

/* PoP: tools_config_run_cua_driver_installer @ hermes_cli/tools_config.py:_run_cua_driver_installer */
bool tools_config_run_cua_driver_installer(const char *label, bool verbose,
                                           const char *pin_version,
                                           bool show_progress);

/* PoP: tools_config_ensure_browser_use_cli @ hermes_cli/tools_config.py:_ensure_browser_use_cli */
void tools_config_ensure_browser_use_cli(bool verbose_hints);

/* PoP: tools_config_run_post_setup @ hermes_cli/tools_config.py:_run_post_setup
 * Runs the post-setup hook for key. Returns 0 on the completed path, 1 when a
 * hook failed (Python prints and returns; callers treat both as non-fatal). */
int tools_config_run_post_setup(const char *post_setup_key);

#endif /* SLERMES_TOOLS_CONFIG_INSTALL_H */
