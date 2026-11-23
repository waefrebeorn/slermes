#ifndef NOC_W15_OLDUPDATER_E72031_H
#define NOC_W15_OLDUPDATER_E72031_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the retired-updater shims in hermes_cli/old_updater_main.py (gate w15_oldupdater_e72031).
 * Outside a historical update, every 'none' shim returns NULL (Python None).
 * Every 'raise' shim returns 'ErrorName|hook' — the class name and hook string
 * passed to stop_historical_updater. Caller frees returned strings. */

/* PoP: w15_oldupdater_e72031_desktop_stamp_path @ hermes_cli/old_updater_main.py:_desktop_stamp_path */
char *w15_oldupdater_e72031_desktop_stamp_path(void);

/* PoP: w15_oldupdater_e72031_expected_windows_pe_machines @ hermes_cli/old_updater_main.py:_expected_windows_pe_machines */
char *w15_oldupdater_e72031_expected_windows_pe_machines(void);

/* PoP: w15_oldupdater_e72031_hermes_exe_shims @ hermes_cli/old_updater_main.py:_hermes_exe_shims */
char *w15_oldupdater_e72031_hermes_exe_shims(void);

/* PoP: w15_oldupdater_e72031_insert_python_pin @ hermes_cli/old_updater_main.py:_insert_python_pin */
char *w15_oldupdater_e72031_insert_python_pin(void);

/* PoP: w15_oldupdater_e72031_interpreter_scripts_dir @ hermes_cli/old_updater_main.py:_interpreter_scripts_dir */
char *w15_oldupdater_e72031_interpreter_scripts_dir(void);

/* PoP: w15_oldupdater_e72031_load_installable_optional_extras @ hermes_cli/old_updater_main.py:_load_installable_optional_extras */
char *w15_oldupdater_e72031_load_installable_optional_extras(void);

/* PoP: w15_oldupdater_e72031_parse_pe_machine @ hermes_cli/old_updater_main.py:_parse_pe_machine */
char *w15_oldupdater_e72031_parse_pe_machine(void);

/* PoP: w15_oldupdater_e72031_quarantine_running_hermes_exe @ hermes_cli/old_updater_main.py:_quarantine_running_hermes_exe */
char *w15_oldupdater_e72031_quarantine_running_hermes_exe(void);

/* PoP: w15_oldupdater_e72031_repair_broken_lazy_refresh_imports @ hermes_cli/old_updater_main.py:_repair_broken_lazy_refresh_imports */
char *w15_oldupdater_e72031_repair_broken_lazy_refresh_imports(void);

/* PoP: w15_oldupdater_e72031_run_install_with_heartbeat @ hermes_cli/old_updater_main.py:_run_install_with_heartbeat */
char *w15_oldupdater_e72031_run_install_with_heartbeat(void);

/* PoP: w15_oldupdater_e72031_run_package_only_install @ hermes_cli/old_updater_main.py:_run_package_only_install */
char *w15_oldupdater_e72031_run_package_only_install(void);

/* PoP: w15_oldupdater_e72031_run_quarantined_install @ hermes_cli/old_updater_main.py:_run_quarantined_install */
char *w15_oldupdater_e72031_run_quarantined_install(void);

/* PoP: w15_oldupdater_e72031_run_with_idle_timeout @ hermes_cli/old_updater_main.py:_run_with_idle_timeout */
char *w15_oldupdater_e72031_run_with_idle_timeout(void);

/* PoP: w15_oldupdater_e72031_self @ hermes_cli/old_updater_main.py:_self */
char *w15_oldupdater_e72031_self(void);

/* PoP: w15_oldupdater_e72031_verify_console_scripts_installed @ hermes_cli/old_updater_main.py:_verify_console_scripts_installed */
char *w15_oldupdater_e72031_verify_console_scripts_installed(void);

/* PoP: w15_oldupdater_e72031_verify_core_dependencies_installed @ hermes_cli/old_updater_main.py:_verify_core_dependencies_installed */
char *w15_oldupdater_e72031_verify_core_dependencies_installed(void);

/* PoP: w15_oldupdater_e72031_web_ui_build_needed @ hermes_cli/old_updater_main.py:_web_ui_build_needed */
char *w15_oldupdater_e72031_web_ui_build_needed(void);

/* PoP: w15_oldupdater_e72031_windows_native_machine @ hermes_cli/old_updater_main.py:_windows_native_machine */
char *w15_oldupdater_e72031_windows_native_machine(void);

/* PoP: w15_oldupdater_e72031_windows_shim_in_process_chain @ hermes_cli/old_updater_main.py:_windows_shim_in_process_chain */
char *w15_oldupdater_e72031_windows_shim_in_process_chain(void);

/* PoP: w15_oldupdater_e72031_detect_broken_lazy_refresh_imports @ hermes_cli/old_updater_main.py:_detect_broken_lazy_refresh_imports */
char *w15_oldupdater_e72031_detect_broken_lazy_refresh_imports(void);

/* PoP: w15_oldupdater_e72031_resolve_install_target_python @ hermes_cli/old_updater_main.py:_resolve_install_target_python */
char *w15_oldupdater_e72031_resolve_install_target_python(void);

/* PoP: w15_oldupdater_e72031_restore_quarantined_exes @ hermes_cli/old_updater_main.py:_restore_quarantined_exes */
char *w15_oldupdater_e72031_restore_quarantined_exes(void);

/* PoP: w15_oldupdater_e72031_write_web_ui_build_stamp @ hermes_cli/old_updater_main.py:_write_web_ui_build_stamp */
char *w15_oldupdater_e72031_write_web_ui_build_stamp(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_OLDUPDATER_E72031_H */
