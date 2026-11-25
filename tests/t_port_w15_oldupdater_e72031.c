#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_oldupdater_e72031.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *s = NULL;
        if (!strcmp(func, "desktop_stamp_path")) {
            s = w15_oldupdater_e72031_desktop_stamp_path();
        }
        else if (!strcmp(func, "expected_windows_pe_machines")) {
            s = w15_oldupdater_e72031_expected_windows_pe_machines();
        }
        else if (!strcmp(func, "hermes_exe_shims")) {
            s = w15_oldupdater_e72031_hermes_exe_shims();
        }
        else if (!strcmp(func, "insert_python_pin")) {
            s = w15_oldupdater_e72031_insert_python_pin();
        }
        else if (!strcmp(func, "interpreter_scripts_dir")) {
            s = w15_oldupdater_e72031_interpreter_scripts_dir();
        }
        else if (!strcmp(func, "load_installable_optional_extras")) {
            s = w15_oldupdater_e72031_load_installable_optional_extras();
        }
        else if (!strcmp(func, "parse_pe_machine")) {
            s = w15_oldupdater_e72031_parse_pe_machine();
        }
        else if (!strcmp(func, "quarantine_running_hermes_exe")) {
            s = w15_oldupdater_e72031_quarantine_running_hermes_exe();
        }
        else if (!strcmp(func, "repair_broken_lazy_refresh_imports")) {
            s = w15_oldupdater_e72031_repair_broken_lazy_refresh_imports();
        }
        else if (!strcmp(func, "run_install_with_heartbeat")) {
            s = w15_oldupdater_e72031_run_install_with_heartbeat();
        }
        else if (!strcmp(func, "run_package_only_install")) {
            s = w15_oldupdater_e72031_run_package_only_install();
        }
        else if (!strcmp(func, "run_quarantined_install")) {
            s = w15_oldupdater_e72031_run_quarantined_install();
        }
        else if (!strcmp(func, "run_with_idle_timeout")) {
            s = w15_oldupdater_e72031_run_with_idle_timeout();
        }
        else if (!strcmp(func, "self")) {
            s = w15_oldupdater_e72031_self();
        }
        else if (!strcmp(func, "verify_console_scripts_installed")) {
            s = w15_oldupdater_e72031_verify_console_scripts_installed();
        }
        else if (!strcmp(func, "verify_core_dependencies_installed")) {
            s = w15_oldupdater_e72031_verify_core_dependencies_installed();
        }
        else if (!strcmp(func, "web_ui_build_needed")) {
            s = w15_oldupdater_e72031_web_ui_build_needed();
        }
        else if (!strcmp(func, "windows_native_machine")) {
            s = w15_oldupdater_e72031_windows_native_machine();
        }
        else if (!strcmp(func, "windows_shim_in_process_chain")) {
            s = w15_oldupdater_e72031_windows_shim_in_process_chain();
        }
        else if (!strcmp(func, "detect_broken_lazy_refresh_imports")) {
            s = w15_oldupdater_e72031_detect_broken_lazy_refresh_imports();
        }
        else if (!strcmp(func, "resolve_install_target_python")) {
            s = w15_oldupdater_e72031_resolve_install_target_python();
        }
        else if (!strcmp(func, "restore_quarantined_exes")) {
            s = w15_oldupdater_e72031_restore_quarantined_exes();
        }
        else if (!strcmp(func, "write_web_ui_build_stamp")) {
            s = w15_oldupdater_e72031_write_web_ui_build_stamp();
        }
        else { out = json_string("UNKNOWN_FUNC"); }
        if (s) { out = json_string(s); free(s); }
        else if (out == NULL) { out = json_null(); }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
