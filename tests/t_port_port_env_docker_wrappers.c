/* AUTO-GENERATED integration oracle harness for port_env_docker_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_env_docker_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int envd_u_normalize_forward_env_names(const char *);
extern int envd_u_normalize_env_dict(const char *);
extern int envd_u_load_hermes_env_vars(const char *);
extern int envd_u_sanitize_label_value(const char *);
extern int envd_u_get_active_profile_name(const char *);
extern int envd_reap_orphan_containers(const char *);
extern int envd_u_container_finished_at(const char *);
extern int envd_find_docker(const char *);
extern int envd_u_egress_proxy_args_for_docker(const char *);
extern int envd_u_egress_reuse_fingerprint(const char *);
extern int envd_u_egress_enforce_on_docker(const char *);
extern int envd_u_critical_egress_env_names(const char *);
extern int envd_u_extra_args_egress_collisions(const char *);
extern int envd_u_build_security_args(const char *);
extern int envd_u_image_uses_init_entrypoint(const char *);
extern int envd_u_resolve_host_user_spec(const char *);
extern int envd_u_cgroup_limits_available(const char *);
extern int envd_u_ensure_docker_available(const char *);
extern int envd_u_build_init_env_args(const char *);
extern int envd_u_is_container_gone(const char *);
extern int envd_u_recreate_container(const char *);
extern int envd_u_storage_opt_supported(const char *);
extern int envd_u_container_network_mode(const char *);
extern int envd_u_find_reusable_container(const char *);
extern int envd_wait_for_cleanup(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_envd_u_normalize_forward_env_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_normalize_forward_env_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_normalize_forward_env_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_normalize_env_dict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_normalize_env_dict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_normalize_env_dict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_load_hermes_env_vars(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_load_hermes_env_vars(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_load_hermes_env_vars"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_sanitize_label_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_sanitize_label_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_sanitize_label_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_get_active_profile_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_get_active_profile_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_get_active_profile_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_reap_orphan_containers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_reap_orphan_containers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_reap_orphan_containers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_container_finished_at(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_container_finished_at(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_container_finished_at"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_find_docker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_find_docker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_find_docker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_egress_proxy_args_for_docker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_egress_proxy_args_for_docker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_egress_proxy_args_for_docker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_egress_reuse_fingerprint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_egress_reuse_fingerprint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_egress_reuse_fingerprint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_egress_enforce_on_docker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_egress_enforce_on_docker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_egress_enforce_on_docker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_critical_egress_env_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_critical_egress_env_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_critical_egress_env_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_extra_args_egress_collisions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_extra_args_egress_collisions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_extra_args_egress_collisions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_build_security_args(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_build_security_args(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_build_security_args"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_image_uses_init_entrypoint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_image_uses_init_entrypoint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_image_uses_init_entrypoint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_resolve_host_user_spec(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_resolve_host_user_spec(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_resolve_host_user_spec"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_cgroup_limits_available(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_cgroup_limits_available(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_cgroup_limits_available"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_ensure_docker_available(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_ensure_docker_available(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_ensure_docker_available"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_build_init_env_args(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_build_init_env_args(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_build_init_env_args"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_is_container_gone(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_is_container_gone(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_is_container_gone"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_recreate_container(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_recreate_container(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_recreate_container"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_storage_opt_supported(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_storage_opt_supported(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_storage_opt_supported"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_container_network_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_container_network_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_container_network_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_u_find_reusable_container(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_u_find_reusable_container(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_u_find_reusable_container"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envd_wait_for_cleanup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envd_wait_for_cleanup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envd_wait_for_cleanup"));
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
        if (strcmp(op, "envd_u_normalize_forward_env_names") == 0) o = emit_envd_u_normalize_forward_env_names(c);
        if (strcmp(op, "envd_u_normalize_env_dict") == 0) o = emit_envd_u_normalize_env_dict(c);
        if (strcmp(op, "envd_u_load_hermes_env_vars") == 0) o = emit_envd_u_load_hermes_env_vars(c);
        if (strcmp(op, "envd_u_sanitize_label_value") == 0) o = emit_envd_u_sanitize_label_value(c);
        if (strcmp(op, "envd_u_get_active_profile_name") == 0) o = emit_envd_u_get_active_profile_name(c);
        if (strcmp(op, "envd_reap_orphan_containers") == 0) o = emit_envd_reap_orphan_containers(c);
        if (strcmp(op, "envd_u_container_finished_at") == 0) o = emit_envd_u_container_finished_at(c);
        if (strcmp(op, "envd_find_docker") == 0) o = emit_envd_find_docker(c);
        if (strcmp(op, "envd_u_egress_proxy_args_for_docker") == 0) o = emit_envd_u_egress_proxy_args_for_docker(c);
        if (strcmp(op, "envd_u_egress_reuse_fingerprint") == 0) o = emit_envd_u_egress_reuse_fingerprint(c);
        if (strcmp(op, "envd_u_egress_enforce_on_docker") == 0) o = emit_envd_u_egress_enforce_on_docker(c);
        if (strcmp(op, "envd_u_critical_egress_env_names") == 0) o = emit_envd_u_critical_egress_env_names(c);
        if (strcmp(op, "envd_u_extra_args_egress_collisions") == 0) o = emit_envd_u_extra_args_egress_collisions(c);
        if (strcmp(op, "envd_u_build_security_args") == 0) o = emit_envd_u_build_security_args(c);
        if (strcmp(op, "envd_u_image_uses_init_entrypoint") == 0) o = emit_envd_u_image_uses_init_entrypoint(c);
        if (strcmp(op, "envd_u_resolve_host_user_spec") == 0) o = emit_envd_u_resolve_host_user_spec(c);
        if (strcmp(op, "envd_u_cgroup_limits_available") == 0) o = emit_envd_u_cgroup_limits_available(c);
        if (strcmp(op, "envd_u_ensure_docker_available") == 0) o = emit_envd_u_ensure_docker_available(c);
        if (strcmp(op, "envd_u_build_init_env_args") == 0) o = emit_envd_u_build_init_env_args(c);
        if (strcmp(op, "envd_u_is_container_gone") == 0) o = emit_envd_u_is_container_gone(c);
        if (strcmp(op, "envd_u_recreate_container") == 0) o = emit_envd_u_recreate_container(c);
        if (strcmp(op, "envd_u_storage_opt_supported") == 0) o = emit_envd_u_storage_opt_supported(c);
        if (strcmp(op, "envd_u_container_network_mode") == 0) o = emit_envd_u_container_network_mode(c);
        if (strcmp(op, "envd_u_find_reusable_container") == 0) o = emit_envd_u_find_reusable_container(c);
        if (strcmp(op, "envd_wait_for_cleanup") == 0) o = emit_envd_wait_for_cleanup(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
