/* AUTO-GENERATED integration oracle harness for port_file_operations (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_file_operations.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool file_ops_has_bom(const char *);
extern bool file_ops_python_delete(const char *);
extern bool file_ops_is_likely_binary(const char *);
extern bool file_ops_is_image(const char *);
extern bool file_ops_file_has_bom(const char *);
extern bool file_ops_is_line_oriented_newline_error(const char *);
extern bool file_ops_pattern_has_regex_newline(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_file_ops_has_bom(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_ops_has_bom(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_ops_has_bom"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_ops_python_delete(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_ops_python_delete(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_ops_python_delete"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_ops_is_likely_binary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_ops_is_likely_binary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_ops_is_likely_binary"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_ops_is_image(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_ops_is_image(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_ops_is_image"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_ops_file_has_bom(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_ops_file_has_bom(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_ops_file_has_bom"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_ops_is_line_oriented_newline_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_ops_is_line_oriented_newline_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_ops_is_line_oriented_newline_error"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_ops_pattern_has_regex_newline(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_ops_pattern_has_regex_newline(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_ops_pattern_has_regex_newline"));
    json_set(o, "out", json_bool(v)); return o;
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
        if (strcmp(op, "file_ops_has_bom") == 0) o = emit_file_ops_has_bom(c);
        if (strcmp(op, "file_ops_python_delete") == 0) o = emit_file_ops_python_delete(c);
        if (strcmp(op, "file_ops_is_likely_binary") == 0) o = emit_file_ops_is_likely_binary(c);
        if (strcmp(op, "file_ops_is_image") == 0) o = emit_file_ops_is_image(c);
        if (strcmp(op, "file_ops_file_has_bom") == 0) o = emit_file_ops_file_has_bom(c);
        if (strcmp(op, "file_ops_is_line_oriented_newline_error") == 0) o = emit_file_ops_is_line_oriented_newline_error(c);
        if (strcmp(op, "file_ops_pattern_has_regex_newline") == 0) o = emit_file_ops_pattern_has_regex_newline(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
