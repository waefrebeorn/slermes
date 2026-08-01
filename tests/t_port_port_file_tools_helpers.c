/* AUTO-GENERATED integration oracle harness for port_file_tools_helpers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_file_tools_helpers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int file_tools_is_internal_file_status_text(const char *);
extern int file_tools_looks_like_read_file_line_numbered_content(const char *);
extern int file_tools_is_internal_file_tool_content(const char *);
extern bool file_tools_uses_container_paths(const char *);
extern bool file_tools_search_result_read_block_error(const char *);
extern bool file_tools_check_file_staleness(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_file_tools_is_internal_file_status_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)file_tools_is_internal_file_status_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_tools_is_internal_file_status_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_file_tools_looks_like_read_file_line_numbered_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)file_tools_looks_like_read_file_line_numbered_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_tools_looks_like_read_file_line_numbered_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_file_tools_is_internal_file_tool_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)file_tools_is_internal_file_tool_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_tools_is_internal_file_tool_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_file_tools_uses_container_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_tools_uses_container_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_tools_uses_container_paths"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_tools_search_result_read_block_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_tools_search_result_read_block_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_tools_search_result_read_block_error"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_file_tools_check_file_staleness(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)file_tools_check_file_staleness(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("file_tools_check_file_staleness"));
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
        if (strcmp(op, "file_tools_is_internal_file_status_text") == 0) o = emit_file_tools_is_internal_file_status_text(c);
        if (strcmp(op, "file_tools_looks_like_read_file_line_numbered_content") == 0) o = emit_file_tools_looks_like_read_file_line_numbered_content(c);
        if (strcmp(op, "file_tools_is_internal_file_tool_content") == 0) o = emit_file_tools_is_internal_file_tool_content(c);
        if (strcmp(op, "file_tools_uses_container_paths") == 0) o = emit_file_tools_uses_container_paths(c);
        if (strcmp(op, "file_tools_search_result_read_block_error") == 0) o = emit_file_tools_search_result_read_block_error(c);
        if (strcmp(op, "file_tools_check_file_staleness") == 0) o = emit_file_tools_check_file_staleness(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
