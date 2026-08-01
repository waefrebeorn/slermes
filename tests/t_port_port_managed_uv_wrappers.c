/* AUTO-GENERATED integration oracle harness for port_managed_uv_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_managed_uv_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int muv_managed_uv_path(const char *);
extern int muv_managed_python_install_dir(const char *);
extern int muv_managed_python_env(const char *);
extern int muv_repaired(const char *);
extern int muv_u_report_runtime_repair_failure(const char *);
extern int muv_u__new__(const char *);
extern int muv_u__iter__(const char *);
extern int muv_u_ensure_uv_path(const char *);
extern int muv_u_venv_python(const char *);
extern int muv_u_remove_tree(const char *);
extern int muv_u_make_world_traversable(const char *);
extern int muv_u_runtime_request(const char *);
extern int muv_u_install_safe_python_generation(const char *);
extern int muv_u_smoke_candidate_venv(const char *);
extern int muv_u_stage_candidate_venv(const char *);
extern int muv_u_rename_with_retry(const char *);
extern int muv_u_cut_over_candidate(const char *);
extern int muv_u_acquire_repair_lock(const char *);
extern int muv_u_release_repair_lock(const char *);
extern int muv_u_windows_runtime_holders(const char *);
extern int muv_repair_vulnerable_runtime(const char *);
extern int muv_u_install_uv(const char *);
extern int muv_u_install_uv_posix(const char *);
extern int muv_u_install_uv_windows(const char *);
extern int muv_rebuild_venv(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_muv_managed_uv_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_managed_uv_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_managed_uv_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_managed_python_install_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_managed_python_install_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_managed_python_install_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_managed_python_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_managed_python_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_managed_python_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_repaired(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_repaired(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_repaired"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_report_runtime_repair_failure(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_report_runtime_repair_failure(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_report_runtime_repair_failure"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u__new__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u__new__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u__new__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u__iter__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u__iter__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u__iter__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_ensure_uv_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_ensure_uv_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_ensure_uv_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_venv_python(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_venv_python(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_venv_python"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_remove_tree(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_remove_tree(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_remove_tree"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_make_world_traversable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_make_world_traversable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_make_world_traversable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_runtime_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_runtime_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_runtime_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_install_safe_python_generation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_install_safe_python_generation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_install_safe_python_generation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_smoke_candidate_venv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_smoke_candidate_venv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_smoke_candidate_venv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_stage_candidate_venv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_stage_candidate_venv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_stage_candidate_venv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_rename_with_retry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_rename_with_retry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_rename_with_retry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_cut_over_candidate(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_cut_over_candidate(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_cut_over_candidate"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_acquire_repair_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_acquire_repair_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_acquire_repair_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_release_repair_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_release_repair_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_release_repair_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_windows_runtime_holders(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_windows_runtime_holders(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_windows_runtime_holders"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_repair_vulnerable_runtime(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_repair_vulnerable_runtime(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_repair_vulnerable_runtime"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_install_uv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_install_uv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_install_uv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_install_uv_posix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_install_uv_posix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_install_uv_posix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_u_install_uv_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_u_install_uv_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_u_install_uv_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_muv_rebuild_venv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)muv_rebuild_venv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("muv_rebuild_venv"));
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
        if (strcmp(op, "muv_managed_uv_path") == 0) o = emit_muv_managed_uv_path(c);
        if (strcmp(op, "muv_managed_python_install_dir") == 0) o = emit_muv_managed_python_install_dir(c);
        if (strcmp(op, "muv_managed_python_env") == 0) o = emit_muv_managed_python_env(c);
        if (strcmp(op, "muv_repaired") == 0) o = emit_muv_repaired(c);
        if (strcmp(op, "muv_u_report_runtime_repair_failure") == 0) o = emit_muv_u_report_runtime_repair_failure(c);
        if (strcmp(op, "muv_u__new__") == 0) o = emit_muv_u__new__(c);
        if (strcmp(op, "muv_u__iter__") == 0) o = emit_muv_u__iter__(c);
        if (strcmp(op, "muv_u_ensure_uv_path") == 0) o = emit_muv_u_ensure_uv_path(c);
        if (strcmp(op, "muv_u_venv_python") == 0) o = emit_muv_u_venv_python(c);
        if (strcmp(op, "muv_u_remove_tree") == 0) o = emit_muv_u_remove_tree(c);
        if (strcmp(op, "muv_u_make_world_traversable") == 0) o = emit_muv_u_make_world_traversable(c);
        if (strcmp(op, "muv_u_runtime_request") == 0) o = emit_muv_u_runtime_request(c);
        if (strcmp(op, "muv_u_install_safe_python_generation") == 0) o = emit_muv_u_install_safe_python_generation(c);
        if (strcmp(op, "muv_u_smoke_candidate_venv") == 0) o = emit_muv_u_smoke_candidate_venv(c);
        if (strcmp(op, "muv_u_stage_candidate_venv") == 0) o = emit_muv_u_stage_candidate_venv(c);
        if (strcmp(op, "muv_u_rename_with_retry") == 0) o = emit_muv_u_rename_with_retry(c);
        if (strcmp(op, "muv_u_cut_over_candidate") == 0) o = emit_muv_u_cut_over_candidate(c);
        if (strcmp(op, "muv_u_acquire_repair_lock") == 0) o = emit_muv_u_acquire_repair_lock(c);
        if (strcmp(op, "muv_u_release_repair_lock") == 0) o = emit_muv_u_release_repair_lock(c);
        if (strcmp(op, "muv_u_windows_runtime_holders") == 0) o = emit_muv_u_windows_runtime_holders(c);
        if (strcmp(op, "muv_repair_vulnerable_runtime") == 0) o = emit_muv_repair_vulnerable_runtime(c);
        if (strcmp(op, "muv_u_install_uv") == 0) o = emit_muv_u_install_uv(c);
        if (strcmp(op, "muv_u_install_uv_posix") == 0) o = emit_muv_u_install_uv_posix(c);
        if (strcmp(op, "muv_u_install_uv_windows") == 0) o = emit_muv_u_install_uv_windows(c);
        if (strcmp(op, "muv_rebuild_venv") == 0) o = emit_muv_rebuild_venv(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
