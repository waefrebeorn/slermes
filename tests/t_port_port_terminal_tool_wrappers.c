/* AUTO-GENERATED integration oracle harness for port_terminal_tool_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_terminal_tool_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int tt_u_safe_parse_import_env(const char *);
extern int tt_u_get_sudo_password_callback(const char *);
extern int tt_u_get_approval_callback(const char *);
extern int tt_u_get_sudo_password_cache_scope(const char *);
extern int tt_u_get_cached_sudo_password(const char *);
extern int tt_u_set_cached_sudo_password(const char *);
extern int tt_u_reset_cached_sudo_passwords(const char *);
extern int tt_u_docker_volume_uses_host_path(const char *);
extern int tt_u_docker_has_host_access(const char *);
extern int tt_u_check_all_guards(const char *);
extern int tt_u_sudo_wrong_password_failure(const char *);
extern int tt_u_invalidate_cached_sudo_on_auth_failure(const char *);
extern int tt_u_count_real_sudo_invocations(const char *);
extern int tt_record_session_cwd(const char *);
extern int tt_get_session_cwd(const char *);
extern int tt_register_task_env_overrides(const char *);
extern int tt_clear_task_env_overrides(const char *);
extern int tt_u_resolve_container_task_id(const char *);
extern int tt_resolve_task_overrides(const char *);
extern int tt_u_parse_env_var(const char *);
extern int tt_u_safe_getcwd(const char *);
extern int tt_u_is_ssh_remote_tilde_cwd(const char *);
extern int tt_u_is_unusable_container_cwd(const char *);
extern int tt_u_ensure_terminal_env_bridged(const char *);
extern int tt_u_get_modal_backend_state(const char *);
extern int tt_u_cleanup_thread_worker(const char *);
extern int tt_u_start_cleanup_thread(const char *);
extern int tt_u_stop_cleanup_thread(const char *);
extern int tt_u_atexit_cleanup(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_tt_u_safe_parse_import_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_safe_parse_import_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_safe_parse_import_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_get_sudo_password_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_get_sudo_password_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_get_sudo_password_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_get_approval_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_get_approval_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_get_approval_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_get_sudo_password_cache_scope(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_get_sudo_password_cache_scope(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_get_sudo_password_cache_scope"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_get_cached_sudo_password(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_get_cached_sudo_password(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_get_cached_sudo_password"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_set_cached_sudo_password(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_set_cached_sudo_password(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_set_cached_sudo_password"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_reset_cached_sudo_passwords(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_reset_cached_sudo_passwords(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_reset_cached_sudo_passwords"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_docker_volume_uses_host_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_docker_volume_uses_host_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_docker_volume_uses_host_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_docker_has_host_access(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_docker_has_host_access(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_docker_has_host_access"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_check_all_guards(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_check_all_guards(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_check_all_guards"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_sudo_wrong_password_failure(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_sudo_wrong_password_failure(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_sudo_wrong_password_failure"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_invalidate_cached_sudo_on_auth_failure(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_invalidate_cached_sudo_on_auth_failure(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_invalidate_cached_sudo_on_auth_failure"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_count_real_sudo_invocations(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_count_real_sudo_invocations(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_count_real_sudo_invocations"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_record_session_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_record_session_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_record_session_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_get_session_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_get_session_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_get_session_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_register_task_env_overrides(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_register_task_env_overrides(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_register_task_env_overrides"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_clear_task_env_overrides(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_clear_task_env_overrides(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_clear_task_env_overrides"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_resolve_container_task_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_resolve_container_task_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_resolve_container_task_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_resolve_task_overrides(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_resolve_task_overrides(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_resolve_task_overrides"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_parse_env_var(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_parse_env_var(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_parse_env_var"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_safe_getcwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_safe_getcwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_safe_getcwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_is_ssh_remote_tilde_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_is_ssh_remote_tilde_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_is_ssh_remote_tilde_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_is_unusable_container_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_is_unusable_container_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_is_unusable_container_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_ensure_terminal_env_bridged(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_ensure_terminal_env_bridged(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_ensure_terminal_env_bridged"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_get_modal_backend_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_get_modal_backend_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_get_modal_backend_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_cleanup_thread_worker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_cleanup_thread_worker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_cleanup_thread_worker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_start_cleanup_thread(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_start_cleanup_thread(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_start_cleanup_thread"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_stop_cleanup_thread(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_stop_cleanup_thread(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_stop_cleanup_thread"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tt_u_atexit_cleanup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tt_u_atexit_cleanup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tt_u_atexit_cleanup"));
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
        if (strcmp(op, "tt_u_safe_parse_import_env") == 0) o = emit_tt_u_safe_parse_import_env(c);
        if (strcmp(op, "tt_u_get_sudo_password_callback") == 0) o = emit_tt_u_get_sudo_password_callback(c);
        if (strcmp(op, "tt_u_get_approval_callback") == 0) o = emit_tt_u_get_approval_callback(c);
        if (strcmp(op, "tt_u_get_sudo_password_cache_scope") == 0) o = emit_tt_u_get_sudo_password_cache_scope(c);
        if (strcmp(op, "tt_u_get_cached_sudo_password") == 0) o = emit_tt_u_get_cached_sudo_password(c);
        if (strcmp(op, "tt_u_set_cached_sudo_password") == 0) o = emit_tt_u_set_cached_sudo_password(c);
        if (strcmp(op, "tt_u_reset_cached_sudo_passwords") == 0) o = emit_tt_u_reset_cached_sudo_passwords(c);
        if (strcmp(op, "tt_u_docker_volume_uses_host_path") == 0) o = emit_tt_u_docker_volume_uses_host_path(c);
        if (strcmp(op, "tt_u_docker_has_host_access") == 0) o = emit_tt_u_docker_has_host_access(c);
        if (strcmp(op, "tt_u_check_all_guards") == 0) o = emit_tt_u_check_all_guards(c);
        if (strcmp(op, "tt_u_sudo_wrong_password_failure") == 0) o = emit_tt_u_sudo_wrong_password_failure(c);
        if (strcmp(op, "tt_u_invalidate_cached_sudo_on_auth_failure") == 0) o = emit_tt_u_invalidate_cached_sudo_on_auth_failure(c);
        if (strcmp(op, "tt_u_count_real_sudo_invocations") == 0) o = emit_tt_u_count_real_sudo_invocations(c);
        if (strcmp(op, "tt_record_session_cwd") == 0) o = emit_tt_record_session_cwd(c);
        if (strcmp(op, "tt_get_session_cwd") == 0) o = emit_tt_get_session_cwd(c);
        if (strcmp(op, "tt_register_task_env_overrides") == 0) o = emit_tt_register_task_env_overrides(c);
        if (strcmp(op, "tt_clear_task_env_overrides") == 0) o = emit_tt_clear_task_env_overrides(c);
        if (strcmp(op, "tt_u_resolve_container_task_id") == 0) o = emit_tt_u_resolve_container_task_id(c);
        if (strcmp(op, "tt_resolve_task_overrides") == 0) o = emit_tt_resolve_task_overrides(c);
        if (strcmp(op, "tt_u_parse_env_var") == 0) o = emit_tt_u_parse_env_var(c);
        if (strcmp(op, "tt_u_safe_getcwd") == 0) o = emit_tt_u_safe_getcwd(c);
        if (strcmp(op, "tt_u_is_ssh_remote_tilde_cwd") == 0) o = emit_tt_u_is_ssh_remote_tilde_cwd(c);
        if (strcmp(op, "tt_u_is_unusable_container_cwd") == 0) o = emit_tt_u_is_unusable_container_cwd(c);
        if (strcmp(op, "tt_u_ensure_terminal_env_bridged") == 0) o = emit_tt_u_ensure_terminal_env_bridged(c);
        if (strcmp(op, "tt_u_get_modal_backend_state") == 0) o = emit_tt_u_get_modal_backend_state(c);
        if (strcmp(op, "tt_u_cleanup_thread_worker") == 0) o = emit_tt_u_cleanup_thread_worker(c);
        if (strcmp(op, "tt_u_start_cleanup_thread") == 0) o = emit_tt_u_start_cleanup_thread(c);
        if (strcmp(op, "tt_u_stop_cleanup_thread") == 0) o = emit_tt_u_stop_cleanup_thread(c);
        if (strcmp(op, "tt_u_atexit_cleanup") == 0) o = emit_tt_u_atexit_cleanup(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
