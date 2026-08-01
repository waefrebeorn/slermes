/* AUTO-GENERATED integration oracle harness for port_status_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_status_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int gstat_u_get_starts_log_path(const char *);
extern int gstat_record_start_and_check_storm(const char *);
extern int gstat_u_get_process_hermes_home(const char *);
extern int gstat_u_canonical_hermes_home(const char *);
extern int gstat_u_same_hermes_home(const char *);
extern int gstat_normalize_updated_at(const char *);
extern int gstat_u_clear_running_pid_cache(const char *);
extern int gstat_u_file_cache_signature(const char *);
extern int gstat_u_running_pid_cache_signature(const char *);
extern int gstat_runtime_status_is_stale(const char *);
extern int gstat_runtime_status_pid_is_live(const char *);
extern int gstat_u_validated_scoped_lock_gateway_owner(const char *);
extern int gstat_u_scoped_lock_owner_state(const char *);
extern int gstat_u_wait_for_scoped_lock_owner_exit(const char *);
extern int gstat_u_snapshot_gateway_children(const char *);
extern int gstat_reap_gateway_children(const char *);
extern int gstat_take_over_scoped_lock_holder(const char *);
extern int gstat_u_terminate_scoped_lock_owner_once(const char *);
extern int gstat_get_running_pid_cached(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_gstat_u_get_starts_log_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_get_starts_log_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_get_starts_log_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_record_start_and_check_storm(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_record_start_and_check_storm(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_record_start_and_check_storm"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_get_process_hermes_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_get_process_hermes_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_get_process_hermes_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_canonical_hermes_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_canonical_hermes_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_canonical_hermes_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_same_hermes_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_same_hermes_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_same_hermes_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_normalize_updated_at(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_normalize_updated_at(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_normalize_updated_at"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_clear_running_pid_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_clear_running_pid_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_clear_running_pid_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_file_cache_signature(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_file_cache_signature(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_file_cache_signature"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_running_pid_cache_signature(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_running_pid_cache_signature(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_running_pid_cache_signature"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_runtime_status_is_stale(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_runtime_status_is_stale(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_runtime_status_is_stale"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_runtime_status_pid_is_live(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_runtime_status_pid_is_live(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_runtime_status_pid_is_live"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_validated_scoped_lock_gateway_owner(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_validated_scoped_lock_gateway_owner(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_validated_scoped_lock_gateway_owner"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_scoped_lock_owner_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_scoped_lock_owner_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_scoped_lock_owner_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_wait_for_scoped_lock_owner_exit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_wait_for_scoped_lock_owner_exit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_wait_for_scoped_lock_owner_exit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_snapshot_gateway_children(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_snapshot_gateway_children(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_snapshot_gateway_children"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_reap_gateway_children(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_reap_gateway_children(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_reap_gateway_children"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_take_over_scoped_lock_holder(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_take_over_scoped_lock_holder(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_take_over_scoped_lock_holder"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_u_terminate_scoped_lock_owner_once(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_u_terminate_scoped_lock_owner_once(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_u_terminate_scoped_lock_owner_once"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gstat_get_running_pid_cached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gstat_get_running_pid_cached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gstat_get_running_pid_cached"));
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
        if (strcmp(op, "gstat_u_get_starts_log_path") == 0) o = emit_gstat_u_get_starts_log_path(c);
        if (strcmp(op, "gstat_record_start_and_check_storm") == 0) o = emit_gstat_record_start_and_check_storm(c);
        if (strcmp(op, "gstat_u_get_process_hermes_home") == 0) o = emit_gstat_u_get_process_hermes_home(c);
        if (strcmp(op, "gstat_u_canonical_hermes_home") == 0) o = emit_gstat_u_canonical_hermes_home(c);
        if (strcmp(op, "gstat_u_same_hermes_home") == 0) o = emit_gstat_u_same_hermes_home(c);
        if (strcmp(op, "gstat_normalize_updated_at") == 0) o = emit_gstat_normalize_updated_at(c);
        if (strcmp(op, "gstat_u_clear_running_pid_cache") == 0) o = emit_gstat_u_clear_running_pid_cache(c);
        if (strcmp(op, "gstat_u_file_cache_signature") == 0) o = emit_gstat_u_file_cache_signature(c);
        if (strcmp(op, "gstat_u_running_pid_cache_signature") == 0) o = emit_gstat_u_running_pid_cache_signature(c);
        if (strcmp(op, "gstat_runtime_status_is_stale") == 0) o = emit_gstat_runtime_status_is_stale(c);
        if (strcmp(op, "gstat_runtime_status_pid_is_live") == 0) o = emit_gstat_runtime_status_pid_is_live(c);
        if (strcmp(op, "gstat_u_validated_scoped_lock_gateway_owner") == 0) o = emit_gstat_u_validated_scoped_lock_gateway_owner(c);
        if (strcmp(op, "gstat_u_scoped_lock_owner_state") == 0) o = emit_gstat_u_scoped_lock_owner_state(c);
        if (strcmp(op, "gstat_u_wait_for_scoped_lock_owner_exit") == 0) o = emit_gstat_u_wait_for_scoped_lock_owner_exit(c);
        if (strcmp(op, "gstat_u_snapshot_gateway_children") == 0) o = emit_gstat_u_snapshot_gateway_children(c);
        if (strcmp(op, "gstat_reap_gateway_children") == 0) o = emit_gstat_reap_gateway_children(c);
        if (strcmp(op, "gstat_take_over_scoped_lock_holder") == 0) o = emit_gstat_take_over_scoped_lock_holder(c);
        if (strcmp(op, "gstat_u_terminate_scoped_lock_owner_once") == 0) o = emit_gstat_u_terminate_scoped_lock_owner_once(c);
        if (strcmp(op, "gstat_get_running_pid_cached") == 0) o = emit_gstat_get_running_pid_cached(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
