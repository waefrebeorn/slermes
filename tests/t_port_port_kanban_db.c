/* AUTO-GENERATED integration oracle harness for port_kanban_db (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_kanban_db.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int profile_exists(const char *);
extern long to_epoch(const char *);
extern int looks_like_path(const char *);
extern int is_windows_batch_shim(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_profile_exists(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)profile_exists(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("profile_exists"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_to_epoch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)to_epoch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("to_epoch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_looks_like_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)looks_like_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("looks_like_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_is_windows_batch_shim(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)is_windows_batch_shim(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("is_windows_batch_shim"));
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
        if (strcmp(op, "profile_exists") == 0) o = emit_profile_exists(c);
        if (strcmp(op, "to_epoch") == 0) o = emit_to_epoch(c);
        if (strcmp(op, "looks_like_path") == 0) o = emit_looks_like_path(c);
        if (strcmp(op, "is_windows_batch_shim") == 0) o = emit_is_windows_batch_shim(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
