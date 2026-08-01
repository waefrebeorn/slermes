/* AUTO-GENERATED integration oracle harness for port_web_server_managed_files (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_web_server_managed_files.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool ws_is_sensitive_filename(const char *);
extern bool ws_is_sensitive_path(const char *);
extern bool ws_chat_image_extension_allowed(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_ws_is_sensitive_filename(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)ws_is_sensitive_filename(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ws_is_sensitive_filename"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_ws_is_sensitive_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)ws_is_sensitive_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ws_is_sensitive_path"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_ws_chat_image_extension_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)ws_chat_image_extension_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ws_chat_image_extension_allowed"));
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
        if (strcmp(op, "ws_is_sensitive_filename") == 0) o = emit_ws_is_sensitive_filename(c);
        if (strcmp(op, "ws_is_sensitive_path") == 0) o = emit_ws_is_sensitive_path(c);
        if (strcmp(op, "ws_chat_image_extension_allowed") == 0) o = emit_ws_chat_image_extension_allowed(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
