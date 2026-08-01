/* AUTO-GENERATED integration oracle harness for port_plugins_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_plugins_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool plug_env_enabled(const char *);
extern bool plug_tool_override_allowed(const char *);
extern bool plug_has_hook(const char *);
extern bool plug_has_middleware(const char *);
extern bool plug_remove_skill(const char *);
extern bool plug_resolve_pre_tool_block(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_plug_env_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)plug_env_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("plug_env_enabled"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_plug_tool_override_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)plug_tool_override_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("plug_tool_override_allowed"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_plug_has_hook(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)plug_has_hook(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("plug_has_hook"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_plug_has_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)plug_has_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("plug_has_middleware"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_plug_remove_skill(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)plug_remove_skill(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("plug_remove_skill"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_plug_resolve_pre_tool_block(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)plug_resolve_pre_tool_block(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("plug_resolve_pre_tool_block"));
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
        if (strcmp(op, "plug_env_enabled") == 0) o = emit_plug_env_enabled(c);
        if (strcmp(op, "plug_tool_override_allowed") == 0) o = emit_plug_tool_override_allowed(c);
        if (strcmp(op, "plug_has_hook") == 0) o = emit_plug_has_hook(c);
        if (strcmp(op, "plug_has_middleware") == 0) o = emit_plug_has_middleware(c);
        if (strcmp(op, "plug_remove_skill") == 0) o = emit_plug_remove_skill(c);
        if (strcmp(op, "plug_resolve_pre_tool_block") == 0) o = emit_plug_resolve_pre_tool_block(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
