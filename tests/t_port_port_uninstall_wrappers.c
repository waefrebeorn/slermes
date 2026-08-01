/* AUTO-GENERATED integration oracle harness for port_uninstall_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_uninstall_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int uninst_log_info(const char *);
extern int uninst_log_success(const char *);
extern int uninst_log_warn(const char *);
extern int uninst_find_shell_configs(const char *);
extern int uninst_remove_path_from_shell_configs(const char *);
extern int uninst_remove_wrapper_script(const char *);
extern int uninst_u_node_symlink_candidate_dirs(const char *);
extern int uninst_remove_node_symlinks(const char *);
extern int uninst_uninstall_gateway_service(const char *);
extern int uninst_u_hermes_path_markers(const char *);
extern int uninst_remove_path_from_windows_registry(const char *);
extern int uninst_remove_hermes_env_vars_windows(const char *);
extern int uninst_remove_portable_tooling_windows(const char *);
extern int uninst_u_is_default_hermes_home(const char *);
extern int uninst_u_discover_named_profiles(const char *);
extern int uninst_u_uninstall_profile(const char *);
extern int uninst_run_gui_uninstall(const char *);
extern int uninst_run_uninstall(const char *);
extern int uninst_u_print_uninstall_dry_run(const char *);
extern int uninst_u_perform_uninstall(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_uninst_log_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_log_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_log_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_log_success(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_log_success(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_log_success"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_log_warn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_log_warn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_log_warn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_find_shell_configs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_find_shell_configs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_find_shell_configs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_remove_path_from_shell_configs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_remove_path_from_shell_configs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_remove_path_from_shell_configs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_remove_wrapper_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_remove_wrapper_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_remove_wrapper_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_u_node_symlink_candidate_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_u_node_symlink_candidate_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_u_node_symlink_candidate_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_remove_node_symlinks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_remove_node_symlinks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_remove_node_symlinks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_uninstall_gateway_service(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_uninstall_gateway_service(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_uninstall_gateway_service"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_u_hermes_path_markers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_u_hermes_path_markers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_u_hermes_path_markers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_remove_path_from_windows_registry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_remove_path_from_windows_registry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_remove_path_from_windows_registry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_remove_hermes_env_vars_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_remove_hermes_env_vars_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_remove_hermes_env_vars_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_remove_portable_tooling_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_remove_portable_tooling_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_remove_portable_tooling_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_u_is_default_hermes_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_u_is_default_hermes_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_u_is_default_hermes_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_u_discover_named_profiles(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_u_discover_named_profiles(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_u_discover_named_profiles"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_u_uninstall_profile(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_u_uninstall_profile(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_u_uninstall_profile"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_run_gui_uninstall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_run_gui_uninstall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_run_gui_uninstall"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_run_uninstall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_run_uninstall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_run_uninstall"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_u_print_uninstall_dry_run(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_u_print_uninstall_dry_run(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_u_print_uninstall_dry_run"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_uninst_u_perform_uninstall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)uninst_u_perform_uninstall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("uninst_u_perform_uninstall"));
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
        if (strcmp(op, "uninst_log_info") == 0) o = emit_uninst_log_info(c);
        if (strcmp(op, "uninst_log_success") == 0) o = emit_uninst_log_success(c);
        if (strcmp(op, "uninst_log_warn") == 0) o = emit_uninst_log_warn(c);
        if (strcmp(op, "uninst_find_shell_configs") == 0) o = emit_uninst_find_shell_configs(c);
        if (strcmp(op, "uninst_remove_path_from_shell_configs") == 0) o = emit_uninst_remove_path_from_shell_configs(c);
        if (strcmp(op, "uninst_remove_wrapper_script") == 0) o = emit_uninst_remove_wrapper_script(c);
        if (strcmp(op, "uninst_u_node_symlink_candidate_dirs") == 0) o = emit_uninst_u_node_symlink_candidate_dirs(c);
        if (strcmp(op, "uninst_remove_node_symlinks") == 0) o = emit_uninst_remove_node_symlinks(c);
        if (strcmp(op, "uninst_uninstall_gateway_service") == 0) o = emit_uninst_uninstall_gateway_service(c);
        if (strcmp(op, "uninst_u_hermes_path_markers") == 0) o = emit_uninst_u_hermes_path_markers(c);
        if (strcmp(op, "uninst_remove_path_from_windows_registry") == 0) o = emit_uninst_remove_path_from_windows_registry(c);
        if (strcmp(op, "uninst_remove_hermes_env_vars_windows") == 0) o = emit_uninst_remove_hermes_env_vars_windows(c);
        if (strcmp(op, "uninst_remove_portable_tooling_windows") == 0) o = emit_uninst_remove_portable_tooling_windows(c);
        if (strcmp(op, "uninst_u_is_default_hermes_home") == 0) o = emit_uninst_u_is_default_hermes_home(c);
        if (strcmp(op, "uninst_u_discover_named_profiles") == 0) o = emit_uninst_u_discover_named_profiles(c);
        if (strcmp(op, "uninst_u_uninstall_profile") == 0) o = emit_uninst_u_uninstall_profile(c);
        if (strcmp(op, "uninst_run_gui_uninstall") == 0) o = emit_uninst_run_gui_uninstall(c);
        if (strcmp(op, "uninst_run_uninstall") == 0) o = emit_uninst_run_uninstall(c);
        if (strcmp(op, "uninst_u_print_uninstall_dry_run") == 0) o = emit_uninst_u_print_uninstall_dry_run(c);
        if (strcmp(op, "uninst_u_perform_uninstall") == 0) o = emit_uninst_u_perform_uninstall(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
