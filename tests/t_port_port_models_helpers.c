/* AUTO-GENERATED integration oracle harness for port_models_helpers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_models_helpers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int base_url_looks_like_anthropic_messages(const char *);
extern int is_openai_fast_model(const char *);
extern int is_anthropic_fast_model(const char *);
extern int model_supports_fast_mode(const char *);
extern int openrouter_model_is_free(const char *);
extern int is_github_models_base_url(const char *);
extern int should_use_copilot_responses_api(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_base_url_looks_like_anthropic_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (base_url_looks_like_anthropic_messages(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("base_url_looks_like_anthropic_messages"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_is_openai_fast_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (is_openai_fast_model(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("is_openai_fast_model"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_is_anthropic_fast_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (is_anthropic_fast_model(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("is_anthropic_fast_model"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_model_supports_fast_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (model_supports_fast_mode(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("model_supports_fast_mode"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_openrouter_model_is_free(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (openrouter_model_is_free(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("openrouter_model_is_free"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_is_github_models_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (is_github_models_base_url(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("is_github_models_base_url"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_should_use_copilot_responses_api(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (should_use_copilot_responses_api(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("should_use_copilot_responses_api"));
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
        if (strcmp(op, "base_url_looks_like_anthropic_messages") == 0) o = emit_base_url_looks_like_anthropic_messages(c);
        else if (strcmp(op, "is_openai_fast_model") == 0) o = emit_is_openai_fast_model(c);
        else if (strcmp(op, "is_anthropic_fast_model") == 0) o = emit_is_anthropic_fast_model(c);
        else if (strcmp(op, "model_supports_fast_mode") == 0) o = emit_model_supports_fast_mode(c);
        else if (strcmp(op, "openrouter_model_is_free") == 0) o = emit_openrouter_model_is_free(c);
        else if (strcmp(op, "is_github_models_base_url") == 0) o = emit_is_github_models_base_url(c);
        else if (strcmp(op, "should_use_copilot_responses_api") == 0) o = emit_should_use_copilot_responses_api(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
