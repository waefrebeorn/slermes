/*
 * old_updater_main.c — real C11 port of the observable interface of
 * upstream Hermes hermes_cli/old_updater_main.py (gate w15_oldupdater_e72031).
 *
 * Each retired shim's entire behaviour is a call to
 * stop_historical_updater(hook, error); outside a historical update that
 * raises error(hook-message). This port returns the error class name and
 * the hook string so a differential oracle can compare exactly which hook
 * each shim selects. Shims that simply return None produce NULL.
 * PoP: w15_oldupdater_e72031_desktop_stamp_path @ hermes_cli/old_updater_main.py:_desktop_stamp_path
 * PoP: w15_oldupdater_e72031_expected_windows_pe_machines @ hermes_cli/old_updater_main.py:_expected_windows_pe_machines
 * PoP: w15_oldupdater_e72031_hermes_exe_shims @ hermes_cli/old_updater_main.py:_hermes_exe_shims
 * PoP: w15_oldupdater_e72031_insert_python_pin @ hermes_cli/old_updater_main.py:_insert_python_pin
 * PoP: w15_oldupdater_e72031_interpreter_scripts_dir @ hermes_cli/old_updater_main.py:_interpreter_scripts_dir
 * PoP: w15_oldupdater_e72031_load_installable_optional_extras @ hermes_cli/old_updater_main.py:_load_installable_optional_extras
 * PoP: w15_oldupdater_e72031_parse_pe_machine @ hermes_cli/old_updater_main.py:_parse_pe_machine
 * PoP: w15_oldupdater_e72031_quarantine_running_hermes_exe @ hermes_cli/old_updater_main.py:_quarantine_running_hermes_exe
 * PoP: w15_oldupdater_e72031_repair_broken_lazy_refresh_imports @ hermes_cli/old_updater_main.py:_repair_broken_lazy_refresh_imports
 * PoP: w15_oldupdater_e72031_run_install_with_heartbeat @ hermes_cli/old_updater_main.py:_run_install_with_heartbeat
 * PoP: w15_oldupdater_e72031_run_package_only_install @ hermes_cli/old_updater_main.py:_run_package_only_install
 * PoP: w15_oldupdater_e72031_run_quarantined_install @ hermes_cli/old_updater_main.py:_run_quarantined_install
 * PoP: w15_oldupdater_e72031_run_with_idle_timeout @ hermes_cli/old_updater_main.py:_run_with_idle_timeout
 * PoP: w15_oldupdater_e72031_self @ hermes_cli/old_updater_main.py:_self
 * PoP: w15_oldupdater_e72031_verify_console_scripts_installed @ hermes_cli/old_updater_main.py:_verify_console_scripts_installed
 * PoP: w15_oldupdater_e72031_verify_core_dependencies_installed @ hermes_cli/old_updater_main.py:_verify_core_dependencies_installed
 * PoP: w15_oldupdater_e72031_web_ui_build_needed @ hermes_cli/old_updater_main.py:_web_ui_build_needed
 * PoP: w15_oldupdater_e72031_windows_native_machine @ hermes_cli/old_updater_main.py:_windows_native_machine
 * PoP: w15_oldupdater_e72031_windows_shim_in_process_chain @ hermes_cli/old_updater_main.py:_windows_shim_in_process_chain
 * PoP: w15_oldupdater_e72031_detect_broken_lazy_refresh_imports @ hermes_cli/old_updater_main.py:_detect_broken_lazy_refresh_imports
 * PoP: w15_oldupdater_e72031_resolve_install_target_python @ hermes_cli/old_updater_main.py:_resolve_install_target_python
 * PoP: w15_oldupdater_e72031_restore_quarantined_exes @ hermes_cli/old_updater_main.py:_restore_quarantined_exes
 * PoP: w15_oldupdater_e72031_write_web_ui_build_stamp @ hermes_cli/old_updater_main.py:_write_web_ui_build_stamp
 */
#include "noc_w15_oldupdater_e72031.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *w15_oldupdater_e72031_compose(const char *err, const char *hook) {
    size_t n = strlen(err) + strlen(hook) + 2;
    char *s = (char *)malloc(n);
    if (s) snprintf(s, n, "%s|%s", err, hook);
    return s;
}

/* PoP: w15_oldupdater_e72031_desktop_stamp_path @ hermes_cli/old_updater_main.py:_desktop_stamp_path */
char *w15_oldupdater_e72031_desktop_stamp_path(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._desktop_stamp_path");
}

/* PoP: w15_oldupdater_e72031_expected_windows_pe_machines @ hermes_cli/old_updater_main.py:_expected_windows_pe_machines */
char *w15_oldupdater_e72031_expected_windows_pe_machines(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._expected_windows_pe_machines");
}

/* PoP: w15_oldupdater_e72031_hermes_exe_shims @ hermes_cli/old_updater_main.py:_hermes_exe_shims */
char *w15_oldupdater_e72031_hermes_exe_shims(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._hermes_exe_shims");
}

/* PoP: w15_oldupdater_e72031_insert_python_pin @ hermes_cli/old_updater_main.py:_insert_python_pin */
char *w15_oldupdater_e72031_insert_python_pin(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._insert_python_pin");
}

/* PoP: w15_oldupdater_e72031_interpreter_scripts_dir @ hermes_cli/old_updater_main.py:_interpreter_scripts_dir */
char *w15_oldupdater_e72031_interpreter_scripts_dir(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._interpreter_scripts_dir");
}

/* PoP: w15_oldupdater_e72031_load_installable_optional_extras @ hermes_cli/old_updater_main.py:_load_installable_optional_extras */
char *w15_oldupdater_e72031_load_installable_optional_extras(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._load_installable_optional_extras");
}

/* PoP: w15_oldupdater_e72031_parse_pe_machine @ hermes_cli/old_updater_main.py:_parse_pe_machine */
char *w15_oldupdater_e72031_parse_pe_machine(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._parse_pe_machine");
}

/* PoP: w15_oldupdater_e72031_quarantine_running_hermes_exe @ hermes_cli/old_updater_main.py:_quarantine_running_hermes_exe */
char *w15_oldupdater_e72031_quarantine_running_hermes_exe(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._quarantine_running_hermes_exe");
}

/* PoP: w15_oldupdater_e72031_repair_broken_lazy_refresh_imports @ hermes_cli/old_updater_main.py:_repair_broken_lazy_refresh_imports */
char *w15_oldupdater_e72031_repair_broken_lazy_refresh_imports(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._repair_broken_lazy_refresh_imports");
}

/* PoP: w15_oldupdater_e72031_run_install_with_heartbeat @ hermes_cli/old_updater_main.py:_run_install_with_heartbeat */
char *w15_oldupdater_e72031_run_install_with_heartbeat(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._run_install_with_heartbeat");
}

/* PoP: w15_oldupdater_e72031_run_package_only_install @ hermes_cli/old_updater_main.py:_run_package_only_install */
char *w15_oldupdater_e72031_run_package_only_install(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._run_package_only_install");
}

/* PoP: w15_oldupdater_e72031_run_quarantined_install @ hermes_cli/old_updater_main.py:_run_quarantined_install */
char *w15_oldupdater_e72031_run_quarantined_install(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._run_quarantined_install");
}

/* PoP: w15_oldupdater_e72031_run_with_idle_timeout @ hermes_cli/old_updater_main.py:_run_with_idle_timeout */
char *w15_oldupdater_e72031_run_with_idle_timeout(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._run_with_idle_timeout");
}

/* PoP: w15_oldupdater_e72031_self @ hermes_cli/old_updater_main.py:_self */
char *w15_oldupdater_e72031_self(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._self");
}

/* PoP: w15_oldupdater_e72031_verify_console_scripts_installed @ hermes_cli/old_updater_main.py:_verify_console_scripts_installed */
char *w15_oldupdater_e72031_verify_console_scripts_installed(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._verify_console_scripts_installed");
}

/* PoP: w15_oldupdater_e72031_verify_core_dependencies_installed @ hermes_cli/old_updater_main.py:_verify_core_dependencies_installed */
char *w15_oldupdater_e72031_verify_core_dependencies_installed(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._verify_core_dependencies_installed");
}

/* PoP: w15_oldupdater_e72031_web_ui_build_needed @ hermes_cli/old_updater_main.py:_web_ui_build_needed */
char *w15_oldupdater_e72031_web_ui_build_needed(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._web_ui_build_needed");
}

/* PoP: w15_oldupdater_e72031_windows_native_machine @ hermes_cli/old_updater_main.py:_windows_native_machine */
char *w15_oldupdater_e72031_windows_native_machine(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._windows_native_machine");
}

/* PoP: w15_oldupdater_e72031_windows_shim_in_process_chain @ hermes_cli/old_updater_main.py:_windows_shim_in_process_chain */
char *w15_oldupdater_e72031_windows_shim_in_process_chain(void) {
    return w15_oldupdater_e72031_compose("RetiredUpdaterHook", "hermes_cli.old_updater_main._windows_shim_in_process_chain");
}

/* PoP: w15_oldupdater_e72031_detect_broken_lazy_refresh_imports @ hermes_cli/old_updater_main.py:_detect_broken_lazy_refresh_imports */
char *w15_oldupdater_e72031_detect_broken_lazy_refresh_imports(void) { (void)0; return NULL; }

/* PoP: w15_oldupdater_e72031_resolve_install_target_python @ hermes_cli/old_updater_main.py:_resolve_install_target_python */
char *w15_oldupdater_e72031_resolve_install_target_python(void) { (void)0; return NULL; }

/* PoP: w15_oldupdater_e72031_restore_quarantined_exes @ hermes_cli/old_updater_main.py:_restore_quarantined_exes */
char *w15_oldupdater_e72031_restore_quarantined_exes(void) { (void)0; return NULL; }

/* PoP: w15_oldupdater_e72031_write_web_ui_build_stamp @ hermes_cli/old_updater_main.py:_write_web_ui_build_stamp */
char *w15_oldupdater_e72031_write_web_ui_build_stamp(void) { (void)0; return NULL; }

