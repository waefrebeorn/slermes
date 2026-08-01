/* AUTO-GENERATED integration oracle harness for port_env_local_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_env_local_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int envl_u_msys_to_windows_path(const char *);
extern int envl_u_resolve_local_initial_cwd(const char *);
extern int envl_u_windows_to_msys_path(const char *);
extern int envl_u_bash_safe_path(const char *);
extern int envl_u_quote_bash_path(const char *);
extern int envl_u_cwd_usable(const char *);
extern int envl_u_resolve_safe_cwd(const char *);
extern int envl_u_build_provider_env_blocklist(const char *);
extern int envl_u_inject_context_hermes_home(const char *);
extern int envl_u_inject_session_context_env(const char *);
extern int envl_u_scrub_delegated_child_kanban_env(const char *);
extern int envl_hermes_subprocess_env(const char *);
extern int envl_u_find_bash(const char *);
extern int envl_u_looks_like_msys_spawn_failure(const char *);
extern int envl_u_mandatory_aslr_enabled(const char *);
extern int envl_u_git_root_from_bash(const char *);
extern int envl_u_git_bash_aslr_help(const char *);
extern int envl_u_bash_starts(const char *);
extern int envl_u_git_bash_bin_dirs(const char *);
extern int envl_u_prepend_git_bash_dirs(const char *);
extern int envl_u_find_shell(const char *);
extern int envl_u_resolve_hermes_bin_dir(const char *);
extern int envl_u_prepend_hermes_bin_dir(const char *);
extern int envl_u_append_missing_sane_path_entries(const char *);
extern int envl_u_apply_windows_msys_bash_env_defaults(const char *);
extern int envl_u_path_env_key(const char *);
extern int envl_u_make_run_env(const char *);
extern int envl_u_read_terminal_shell_init_config(const char *);
extern int envl_u_resolve_shell_init_files(const char *);
extern int envl_u_prepend_shell_init(const char *);
extern int envl_get_temp_dir(const char *);
extern int envl_u_quote_cwd_for_cd(const char *);
extern int envl_u_quote_shell_path(const char *);
extern int envl_u_update_cwd(const char *);
extern int envl_u_extract_cwd_from_output(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_envl_u_msys_to_windows_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_msys_to_windows_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_msys_to_windows_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_resolve_local_initial_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_resolve_local_initial_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_resolve_local_initial_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_windows_to_msys_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_windows_to_msys_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_windows_to_msys_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_bash_safe_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_bash_safe_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_bash_safe_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_quote_bash_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_quote_bash_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_quote_bash_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_cwd_usable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_cwd_usable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_cwd_usable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_resolve_safe_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_resolve_safe_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_resolve_safe_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_build_provider_env_blocklist(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_build_provider_env_blocklist(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_build_provider_env_blocklist"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_inject_context_hermes_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_inject_context_hermes_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_inject_context_hermes_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_inject_session_context_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_inject_session_context_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_inject_session_context_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_scrub_delegated_child_kanban_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_scrub_delegated_child_kanban_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_scrub_delegated_child_kanban_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_hermes_subprocess_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_hermes_subprocess_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_hermes_subprocess_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_find_bash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_find_bash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_find_bash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_looks_like_msys_spawn_failure(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_looks_like_msys_spawn_failure(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_looks_like_msys_spawn_failure"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_mandatory_aslr_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_mandatory_aslr_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_mandatory_aslr_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_git_root_from_bash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_git_root_from_bash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_git_root_from_bash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_git_bash_aslr_help(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_git_bash_aslr_help(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_git_bash_aslr_help"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_bash_starts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_bash_starts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_bash_starts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_git_bash_bin_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_git_bash_bin_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_git_bash_bin_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_prepend_git_bash_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_prepend_git_bash_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_prepend_git_bash_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_find_shell(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_find_shell(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_find_shell"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_resolve_hermes_bin_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_resolve_hermes_bin_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_resolve_hermes_bin_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_prepend_hermes_bin_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_prepend_hermes_bin_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_prepend_hermes_bin_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_append_missing_sane_path_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_append_missing_sane_path_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_append_missing_sane_path_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_apply_windows_msys_bash_env_defaults(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_apply_windows_msys_bash_env_defaults(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_apply_windows_msys_bash_env_defaults"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_path_env_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_path_env_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_path_env_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_make_run_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_make_run_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_make_run_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_read_terminal_shell_init_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_read_terminal_shell_init_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_read_terminal_shell_init_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_resolve_shell_init_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_resolve_shell_init_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_resolve_shell_init_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_prepend_shell_init(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_prepend_shell_init(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_prepend_shell_init"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_get_temp_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_get_temp_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_get_temp_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_quote_cwd_for_cd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_quote_cwd_for_cd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_quote_cwd_for_cd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_quote_shell_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_quote_shell_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_quote_shell_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_update_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_update_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_update_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envl_u_extract_cwd_from_output(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envl_u_extract_cwd_from_output(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envl_u_extract_cwd_from_output"));
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
        if (strcmp(op, "envl_u_msys_to_windows_path") == 0) o = emit_envl_u_msys_to_windows_path(c);
        if (strcmp(op, "envl_u_resolve_local_initial_cwd") == 0) o = emit_envl_u_resolve_local_initial_cwd(c);
        if (strcmp(op, "envl_u_windows_to_msys_path") == 0) o = emit_envl_u_windows_to_msys_path(c);
        if (strcmp(op, "envl_u_bash_safe_path") == 0) o = emit_envl_u_bash_safe_path(c);
        if (strcmp(op, "envl_u_quote_bash_path") == 0) o = emit_envl_u_quote_bash_path(c);
        if (strcmp(op, "envl_u_cwd_usable") == 0) o = emit_envl_u_cwd_usable(c);
        if (strcmp(op, "envl_u_resolve_safe_cwd") == 0) o = emit_envl_u_resolve_safe_cwd(c);
        if (strcmp(op, "envl_u_build_provider_env_blocklist") == 0) o = emit_envl_u_build_provider_env_blocklist(c);
        if (strcmp(op, "envl_u_inject_context_hermes_home") == 0) o = emit_envl_u_inject_context_hermes_home(c);
        if (strcmp(op, "envl_u_inject_session_context_env") == 0) o = emit_envl_u_inject_session_context_env(c);
        if (strcmp(op, "envl_u_scrub_delegated_child_kanban_env") == 0) o = emit_envl_u_scrub_delegated_child_kanban_env(c);
        if (strcmp(op, "envl_hermes_subprocess_env") == 0) o = emit_envl_hermes_subprocess_env(c);
        if (strcmp(op, "envl_u_find_bash") == 0) o = emit_envl_u_find_bash(c);
        if (strcmp(op, "envl_u_looks_like_msys_spawn_failure") == 0) o = emit_envl_u_looks_like_msys_spawn_failure(c);
        if (strcmp(op, "envl_u_mandatory_aslr_enabled") == 0) o = emit_envl_u_mandatory_aslr_enabled(c);
        if (strcmp(op, "envl_u_git_root_from_bash") == 0) o = emit_envl_u_git_root_from_bash(c);
        if (strcmp(op, "envl_u_git_bash_aslr_help") == 0) o = emit_envl_u_git_bash_aslr_help(c);
        if (strcmp(op, "envl_u_bash_starts") == 0) o = emit_envl_u_bash_starts(c);
        if (strcmp(op, "envl_u_git_bash_bin_dirs") == 0) o = emit_envl_u_git_bash_bin_dirs(c);
        if (strcmp(op, "envl_u_prepend_git_bash_dirs") == 0) o = emit_envl_u_prepend_git_bash_dirs(c);
        if (strcmp(op, "envl_u_find_shell") == 0) o = emit_envl_u_find_shell(c);
        if (strcmp(op, "envl_u_resolve_hermes_bin_dir") == 0) o = emit_envl_u_resolve_hermes_bin_dir(c);
        if (strcmp(op, "envl_u_prepend_hermes_bin_dir") == 0) o = emit_envl_u_prepend_hermes_bin_dir(c);
        if (strcmp(op, "envl_u_append_missing_sane_path_entries") == 0) o = emit_envl_u_append_missing_sane_path_entries(c);
        if (strcmp(op, "envl_u_apply_windows_msys_bash_env_defaults") == 0) o = emit_envl_u_apply_windows_msys_bash_env_defaults(c);
        if (strcmp(op, "envl_u_path_env_key") == 0) o = emit_envl_u_path_env_key(c);
        if (strcmp(op, "envl_u_make_run_env") == 0) o = emit_envl_u_make_run_env(c);
        if (strcmp(op, "envl_u_read_terminal_shell_init_config") == 0) o = emit_envl_u_read_terminal_shell_init_config(c);
        if (strcmp(op, "envl_u_resolve_shell_init_files") == 0) o = emit_envl_u_resolve_shell_init_files(c);
        if (strcmp(op, "envl_u_prepend_shell_init") == 0) o = emit_envl_u_prepend_shell_init(c);
        if (strcmp(op, "envl_get_temp_dir") == 0) o = emit_envl_get_temp_dir(c);
        if (strcmp(op, "envl_u_quote_cwd_for_cd") == 0) o = emit_envl_u_quote_cwd_for_cd(c);
        if (strcmp(op, "envl_u_quote_shell_path") == 0) o = emit_envl_u_quote_shell_path(c);
        if (strcmp(op, "envl_u_update_cwd") == 0) o = emit_envl_u_update_cwd(c);
        if (strcmp(op, "envl_u_extract_cwd_from_output") == 0) o = emit_envl_u_extract_cwd_from_output(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
