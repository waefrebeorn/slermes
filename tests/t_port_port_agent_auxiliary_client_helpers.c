/* AUTO-GENERATED integration oracle harness for port_agent_auxiliary_client_helpers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_agent_auxiliary_client_helpers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int aux__is_anthropic_compatible_host(const char *);
extern int aux__is_invalid_aux_response_error(const char *);
extern int aux__resolve_aux_verify(const char *);
extern int aux__maybe_wrap_anthropic(const char *);
extern int aux__evict_cached_clients(const char *);
extern int aux__recoverable_pool_provider(const char *);
extern int aux__recover_provider_pool(const char *);
extern int aux__is_openrouter_client(const char *);
extern int aux__cached_client_accepts_slash_models(const char *);
extern int aux__refresh_provider_credentials(const char *);
extern int aux__task_minimum_context_length(const char *);
extern int aux__validate_llm_response(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_aux__is_anthropic_compatible_host(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__is_anthropic_compatible_host(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__is_anthropic_compatible_host"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__is_invalid_aux_response_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__is_invalid_aux_response_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__is_invalid_aux_response_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__resolve_aux_verify(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__resolve_aux_verify(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__resolve_aux_verify"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__maybe_wrap_anthropic(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__maybe_wrap_anthropic(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__maybe_wrap_anthropic"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__evict_cached_clients(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__evict_cached_clients(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__evict_cached_clients"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__recoverable_pool_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__recoverable_pool_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__recoverable_pool_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__recover_provider_pool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__recover_provider_pool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__recover_provider_pool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__is_openrouter_client(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__is_openrouter_client(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__is_openrouter_client"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__cached_client_accepts_slash_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__cached_client_accepts_slash_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__cached_client_accepts_slash_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__refresh_provider_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__refresh_provider_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__refresh_provider_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__task_minimum_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__task_minimum_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__task_minimum_context_length"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_aux__validate_llm_response(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__validate_llm_response(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__validate_llm_response"));
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
        if (strcmp(op, "aux__is_anthropic_compatible_host") == 0) o = emit_aux__is_anthropic_compatible_host(c);
        if (strcmp(op, "aux__is_invalid_aux_response_error") == 0) o = emit_aux__is_invalid_aux_response_error(c);
        if (strcmp(op, "aux__resolve_aux_verify") == 0) o = emit_aux__resolve_aux_verify(c);
        if (strcmp(op, "aux__maybe_wrap_anthropic") == 0) o = emit_aux__maybe_wrap_anthropic(c);
        if (strcmp(op, "aux__evict_cached_clients") == 0) o = emit_aux__evict_cached_clients(c);
        if (strcmp(op, "aux__recoverable_pool_provider") == 0) o = emit_aux__recoverable_pool_provider(c);
        if (strcmp(op, "aux__recover_provider_pool") == 0) o = emit_aux__recover_provider_pool(c);
        if (strcmp(op, "aux__is_openrouter_client") == 0) o = emit_aux__is_openrouter_client(c);
        if (strcmp(op, "aux__cached_client_accepts_slash_models") == 0) o = emit_aux__cached_client_accepts_slash_models(c);
        if (strcmp(op, "aux__refresh_provider_credentials") == 0) o = emit_aux__refresh_provider_credentials(c);
        if (strcmp(op, "aux__task_minimum_context_length") == 0) o = emit_aux__task_minimum_context_length(c);
        if (strcmp(op, "aux__validate_llm_response") == 0) o = emit_aux__validate_llm_response(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
