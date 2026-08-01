/* AUTO-GENERATED integration oracle harness for port_windows_ssh_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_windows_ssh_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int wssr_u_win32(const char *);
extern int wssr_u_ownership(const char *);
extern int wssr_u_nonce(const char *);
extern int wssr_u_root(const char *);
extern int wssr_u_directory(const char *);
extern int wssr_u_log_path(const char *);
extern int wssr_u_current_sid(const char *);
extern int wssr_u_system_sid(const char *);
extern int wssr_u_security_attributes(const char *);
extern int wssr_u_allowed_sids(const char *);
extern int wssr_u_verify_security(const char *);
extern int wssr_u_open(const char *);
extern int wssr_u_ensure_directory(const char *);
extern int wssr_u_ensure_scope(const char *);
extern int wssr_upload_token(const char *);
extern int wssr_read_token(const char *);
extern int wssr_u_read_json_stdin(const char *);
extern int wssr_read_lock(const char *);
extern int wssr_write_lock(const char *);
extern int wssr_remove_artifact(const char *);
extern int wssr_process_state(const char *);
extern int wssr_terminate_owned(const char *);
extern int wssr_u_resolve_direct_interpreter(const char *);
extern int wssr_spawn_backend(const char *);
extern int wssr_inspect_hermes(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_wssr_u_win32(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_win32(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_win32"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_ownership(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_ownership(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_ownership"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_nonce(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_nonce(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_nonce"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_directory(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_directory(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_directory"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_log_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_log_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_log_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_current_sid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_current_sid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_current_sid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_system_sid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_system_sid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_system_sid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_security_attributes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_security_attributes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_security_attributes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_allowed_sids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_allowed_sids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_allowed_sids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_verify_security(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_verify_security(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_verify_security"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_open(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_open(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_open"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_ensure_directory(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_ensure_directory(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_ensure_directory"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_ensure_scope(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_ensure_scope(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_ensure_scope"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_upload_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_upload_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_upload_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_read_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_read_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_read_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_read_json_stdin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_read_json_stdin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_read_json_stdin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_read_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_read_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_read_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_write_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_write_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_write_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_remove_artifact(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_remove_artifact(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_remove_artifact"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_process_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_process_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_process_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_terminate_owned(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_terminate_owned(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_terminate_owned"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_u_resolve_direct_interpreter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_u_resolve_direct_interpreter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_u_resolve_direct_interpreter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_spawn_backend(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_spawn_backend(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_spawn_backend"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_wssr_inspect_hermes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)wssr_inspect_hermes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("wssr_inspect_hermes"));
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
        if (strcmp(op, "wssr_u_win32") == 0) o = emit_wssr_u_win32(c);
        if (strcmp(op, "wssr_u_ownership") == 0) o = emit_wssr_u_ownership(c);
        if (strcmp(op, "wssr_u_nonce") == 0) o = emit_wssr_u_nonce(c);
        if (strcmp(op, "wssr_u_root") == 0) o = emit_wssr_u_root(c);
        if (strcmp(op, "wssr_u_directory") == 0) o = emit_wssr_u_directory(c);
        if (strcmp(op, "wssr_u_log_path") == 0) o = emit_wssr_u_log_path(c);
        if (strcmp(op, "wssr_u_current_sid") == 0) o = emit_wssr_u_current_sid(c);
        if (strcmp(op, "wssr_u_system_sid") == 0) o = emit_wssr_u_system_sid(c);
        if (strcmp(op, "wssr_u_security_attributes") == 0) o = emit_wssr_u_security_attributes(c);
        if (strcmp(op, "wssr_u_allowed_sids") == 0) o = emit_wssr_u_allowed_sids(c);
        if (strcmp(op, "wssr_u_verify_security") == 0) o = emit_wssr_u_verify_security(c);
        if (strcmp(op, "wssr_u_open") == 0) o = emit_wssr_u_open(c);
        if (strcmp(op, "wssr_u_ensure_directory") == 0) o = emit_wssr_u_ensure_directory(c);
        if (strcmp(op, "wssr_u_ensure_scope") == 0) o = emit_wssr_u_ensure_scope(c);
        if (strcmp(op, "wssr_upload_token") == 0) o = emit_wssr_upload_token(c);
        if (strcmp(op, "wssr_read_token") == 0) o = emit_wssr_read_token(c);
        if (strcmp(op, "wssr_u_read_json_stdin") == 0) o = emit_wssr_u_read_json_stdin(c);
        if (strcmp(op, "wssr_read_lock") == 0) o = emit_wssr_read_lock(c);
        if (strcmp(op, "wssr_write_lock") == 0) o = emit_wssr_write_lock(c);
        if (strcmp(op, "wssr_remove_artifact") == 0) o = emit_wssr_remove_artifact(c);
        if (strcmp(op, "wssr_process_state") == 0) o = emit_wssr_process_state(c);
        if (strcmp(op, "wssr_terminate_owned") == 0) o = emit_wssr_terminate_owned(c);
        if (strcmp(op, "wssr_u_resolve_direct_interpreter") == 0) o = emit_wssr_u_resolve_direct_interpreter(c);
        if (strcmp(op, "wssr_spawn_backend") == 0) o = emit_wssr_spawn_backend(c);
        if (strcmp(op, "wssr_inspect_hermes") == 0) o = emit_wssr_inspect_hermes(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
