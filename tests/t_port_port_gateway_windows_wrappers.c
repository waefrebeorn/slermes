/* AUTO-GENERATED integration oracle harness for port_gateway_windows_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_gateway_windows_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int gw_u_schtasks_encoding(const char *);
extern int gw_u_assert_windows(const char *);
extern int gw_u_preserve_hermes_home_path(const char *);
extern int gw_u_quote_cmd_script_arg(const char *);
extern int gw_u_quote_schtasks_arg(const char *);
extern int gw_u_exec_schtasks(const char *);
extern int gw_u_should_fall_back(const char *);
extern int gw_u_is_access_denied(const char *);
extern int gw_u_is_running_as_admin(const char *);
extern int gw_u_current_profile_cli_args(const char *);
extern int gw_u_launch_elevated_gateway_command(const char *);
extern int gw_u_launch_elevated_install(const char *);
extern int gw_u_launch_elevated_uninstall(const char *);
extern int gw_get_task_name(const char *);
extern int gw_u_sanitize_filename(const char *);
extern int gw_get_task_script_path(const char *);
extern int gw_u_startup_dir(const char *);
extern int gw_get_startup_entry_path(const char *);
extern int gw_u_legacy_startup_entry_path(const char *);
extern int gw_u_stable_gateway_working_dir(const char *);
extern int gw_u_build_gateway_cmd_script(const char *);
extern int gw_u_quote_vbs_string(const char *);
extern int gw_u_build_gateway_vbs_script(const char *);
extern int gw_u_build_startup_launcher(const char *);
extern int gw_u_write_task_script(const char *);
extern int gw_u_resolve_task_user(const char *);
extern int gw_u_build_scheduled_task_xml(const char *);
extern int gw_u_write_scheduled_task_xml(const char *);
extern int gw_u_install_scheduled_task(const char *);
extern int gw_u_install_startup_entry(const char *);
extern int gw_u_resolve_detached_python(const char *);
extern int gw_u_prepend_pythonpath(const char *);
extern int gw_u_build_gateway_argv(const char *);
extern int gw_windowless_gateway_restart_spec(const char *);
extern int gw_u_spawn_detached(const char *);
extern int gw_u_install_choice_from_env(const char *);
extern int gw_u_prompt_install_choices(const char *);
extern int gw_u_install_startup_fallback(const char *);
extern int gw_u_wait_for_gateway_ready(const char *);
extern int gw_u_report_gateway_start(const char *);
extern int gw_u_print_next_steps(const char *);
extern int gw_is_task_registered(const char *);
extern int gw_is_startup_entry_installed(const char *);
extern int gw_query_task_status(const char *);
extern int gw_u_gateway_pids(const char *);
extern int gw_u_print_deep_probes(const char *);
extern int gw_u_drain_gateway_pid(const char *);
extern int gw_u_windows_stop_drain_timeout(const char *);
extern int gw_u_force_terminate_known_gateway_pids(const char *);
extern int gw_u_collect_gateway_stop_pids(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_gw_u_schtasks_encoding(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_schtasks_encoding(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_schtasks_encoding"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_assert_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_assert_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_assert_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_preserve_hermes_home_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_preserve_hermes_home_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_preserve_hermes_home_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_quote_cmd_script_arg(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_quote_cmd_script_arg(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_quote_cmd_script_arg"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_quote_schtasks_arg(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_quote_schtasks_arg(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_quote_schtasks_arg"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_exec_schtasks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_exec_schtasks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_exec_schtasks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_should_fall_back(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_should_fall_back(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_should_fall_back"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_is_access_denied(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_is_access_denied(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_is_access_denied"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_is_running_as_admin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_is_running_as_admin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_is_running_as_admin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_current_profile_cli_args(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_current_profile_cli_args(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_current_profile_cli_args"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_launch_elevated_gateway_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_launch_elevated_gateway_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_launch_elevated_gateway_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_launch_elevated_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_launch_elevated_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_launch_elevated_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_launch_elevated_uninstall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_launch_elevated_uninstall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_launch_elevated_uninstall"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_get_task_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_get_task_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_get_task_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_sanitize_filename(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_sanitize_filename(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_sanitize_filename"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_get_task_script_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_get_task_script_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_get_task_script_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_startup_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_startup_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_startup_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_get_startup_entry_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_get_startup_entry_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_get_startup_entry_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_legacy_startup_entry_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_legacy_startup_entry_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_legacy_startup_entry_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_stable_gateway_working_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_stable_gateway_working_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_stable_gateway_working_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_build_gateway_cmd_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_build_gateway_cmd_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_build_gateway_cmd_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_quote_vbs_string(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_quote_vbs_string(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_quote_vbs_string"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_build_gateway_vbs_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_build_gateway_vbs_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_build_gateway_vbs_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_build_startup_launcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_build_startup_launcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_build_startup_launcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_write_task_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_write_task_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_write_task_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_resolve_task_user(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_resolve_task_user(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_resolve_task_user"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_build_scheduled_task_xml(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_build_scheduled_task_xml(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_build_scheduled_task_xml"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_write_scheduled_task_xml(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_write_scheduled_task_xml(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_write_scheduled_task_xml"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_install_scheduled_task(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_install_scheduled_task(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_install_scheduled_task"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_install_startup_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_install_startup_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_install_startup_entry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_resolve_detached_python(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_resolve_detached_python(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_resolve_detached_python"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_prepend_pythonpath(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_prepend_pythonpath(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_prepend_pythonpath"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_build_gateway_argv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_build_gateway_argv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_build_gateway_argv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_windowless_gateway_restart_spec(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_windowless_gateway_restart_spec(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_windowless_gateway_restart_spec"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_spawn_detached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_spawn_detached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_spawn_detached"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_install_choice_from_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_install_choice_from_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_install_choice_from_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_prompt_install_choices(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_prompt_install_choices(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_prompt_install_choices"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_install_startup_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_install_startup_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_install_startup_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_wait_for_gateway_ready(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_wait_for_gateway_ready(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_wait_for_gateway_ready"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_report_gateway_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_report_gateway_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_report_gateway_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_print_next_steps(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_print_next_steps(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_print_next_steps"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_is_task_registered(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_is_task_registered(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_is_task_registered"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_is_startup_entry_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_is_startup_entry_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_is_startup_entry_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_query_task_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_query_task_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_query_task_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_gateway_pids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_gateway_pids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_gateway_pids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_print_deep_probes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_print_deep_probes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_print_deep_probes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_drain_gateway_pid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_drain_gateway_pid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_drain_gateway_pid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_windows_stop_drain_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_windows_stop_drain_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_windows_stop_drain_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_force_terminate_known_gateway_pids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_force_terminate_known_gateway_pids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_force_terminate_known_gateway_pids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gw_u_collect_gateway_stop_pids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gw_u_collect_gateway_stop_pids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gw_u_collect_gateway_stop_pids"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "gw_u_schtasks_encoding") == 0) o = emit_gw_u_schtasks_encoding(c);
        if (strcmp(op, "gw_u_assert_windows") == 0) o = emit_gw_u_assert_windows(c);
        if (strcmp(op, "gw_u_preserve_hermes_home_path") == 0) o = emit_gw_u_preserve_hermes_home_path(c);
        if (strcmp(op, "gw_u_quote_cmd_script_arg") == 0) o = emit_gw_u_quote_cmd_script_arg(c);
        if (strcmp(op, "gw_u_quote_schtasks_arg") == 0) o = emit_gw_u_quote_schtasks_arg(c);
        if (strcmp(op, "gw_u_exec_schtasks") == 0) o = emit_gw_u_exec_schtasks(c);
        if (strcmp(op, "gw_u_should_fall_back") == 0) o = emit_gw_u_should_fall_back(c);
        if (strcmp(op, "gw_u_is_access_denied") == 0) o = emit_gw_u_is_access_denied(c);
        if (strcmp(op, "gw_u_is_running_as_admin") == 0) o = emit_gw_u_is_running_as_admin(c);
        if (strcmp(op, "gw_u_current_profile_cli_args") == 0) o = emit_gw_u_current_profile_cli_args(c);
        if (strcmp(op, "gw_u_launch_elevated_gateway_command") == 0) o = emit_gw_u_launch_elevated_gateway_command(c);
        if (strcmp(op, "gw_u_launch_elevated_install") == 0) o = emit_gw_u_launch_elevated_install(c);
        if (strcmp(op, "gw_u_launch_elevated_uninstall") == 0) o = emit_gw_u_launch_elevated_uninstall(c);
        if (strcmp(op, "gw_get_task_name") == 0) o = emit_gw_get_task_name(c);
        if (strcmp(op, "gw_u_sanitize_filename") == 0) o = emit_gw_u_sanitize_filename(c);
        if (strcmp(op, "gw_get_task_script_path") == 0) o = emit_gw_get_task_script_path(c);
        if (strcmp(op, "gw_u_startup_dir") == 0) o = emit_gw_u_startup_dir(c);
        if (strcmp(op, "gw_get_startup_entry_path") == 0) o = emit_gw_get_startup_entry_path(c);
        if (strcmp(op, "gw_u_legacy_startup_entry_path") == 0) o = emit_gw_u_legacy_startup_entry_path(c);
        if (strcmp(op, "gw_u_stable_gateway_working_dir") == 0) o = emit_gw_u_stable_gateway_working_dir(c);
        if (strcmp(op, "gw_u_build_gateway_cmd_script") == 0) o = emit_gw_u_build_gateway_cmd_script(c);
        if (strcmp(op, "gw_u_quote_vbs_string") == 0) o = emit_gw_u_quote_vbs_string(c);
        if (strcmp(op, "gw_u_build_gateway_vbs_script") == 0) o = emit_gw_u_build_gateway_vbs_script(c);
        if (strcmp(op, "gw_u_build_startup_launcher") == 0) o = emit_gw_u_build_startup_launcher(c);
        if (strcmp(op, "gw_u_write_task_script") == 0) o = emit_gw_u_write_task_script(c);
        if (strcmp(op, "gw_u_resolve_task_user") == 0) o = emit_gw_u_resolve_task_user(c);
        if (strcmp(op, "gw_u_build_scheduled_task_xml") == 0) o = emit_gw_u_build_scheduled_task_xml(c);
        if (strcmp(op, "gw_u_write_scheduled_task_xml") == 0) o = emit_gw_u_write_scheduled_task_xml(c);
        if (strcmp(op, "gw_u_install_scheduled_task") == 0) o = emit_gw_u_install_scheduled_task(c);
        if (strcmp(op, "gw_u_install_startup_entry") == 0) o = emit_gw_u_install_startup_entry(c);
        if (strcmp(op, "gw_u_resolve_detached_python") == 0) o = emit_gw_u_resolve_detached_python(c);
        if (strcmp(op, "gw_u_prepend_pythonpath") == 0) o = emit_gw_u_prepend_pythonpath(c);
        if (strcmp(op, "gw_u_build_gateway_argv") == 0) o = emit_gw_u_build_gateway_argv(c);
        if (strcmp(op, "gw_windowless_gateway_restart_spec") == 0) o = emit_gw_windowless_gateway_restart_spec(c);
        if (strcmp(op, "gw_u_spawn_detached") == 0) o = emit_gw_u_spawn_detached(c);
        if (strcmp(op, "gw_u_install_choice_from_env") == 0) o = emit_gw_u_install_choice_from_env(c);
        if (strcmp(op, "gw_u_prompt_install_choices") == 0) o = emit_gw_u_prompt_install_choices(c);
        if (strcmp(op, "gw_u_install_startup_fallback") == 0) o = emit_gw_u_install_startup_fallback(c);
        if (strcmp(op, "gw_u_wait_for_gateway_ready") == 0) o = emit_gw_u_wait_for_gateway_ready(c);
        if (strcmp(op, "gw_u_report_gateway_start") == 0) o = emit_gw_u_report_gateway_start(c);
        if (strcmp(op, "gw_u_print_next_steps") == 0) o = emit_gw_u_print_next_steps(c);
        if (strcmp(op, "gw_is_task_registered") == 0) o = emit_gw_is_task_registered(c);
        if (strcmp(op, "gw_is_startup_entry_installed") == 0) o = emit_gw_is_startup_entry_installed(c);
        if (strcmp(op, "gw_query_task_status") == 0) o = emit_gw_query_task_status(c);
        if (strcmp(op, "gw_u_gateway_pids") == 0) o = emit_gw_u_gateway_pids(c);
        if (strcmp(op, "gw_u_print_deep_probes") == 0) o = emit_gw_u_print_deep_probes(c);
        if (strcmp(op, "gw_u_drain_gateway_pid") == 0) o = emit_gw_u_drain_gateway_pid(c);
        if (strcmp(op, "gw_u_windows_stop_drain_timeout") == 0) o = emit_gw_u_windows_stop_drain_timeout(c);
        if (strcmp(op, "gw_u_force_terminate_known_gateway_pids") == 0) o = emit_gw_u_force_terminate_known_gateway_pids(c);
        if (strcmp(op, "gw_u_collect_gateway_stop_pids") == 0) o = emit_gw_u_collect_gateway_stop_pids(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
