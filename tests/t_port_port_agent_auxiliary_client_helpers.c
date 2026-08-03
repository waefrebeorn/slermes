/* AUTO-GENERATED integration oracle harness for port_agent_auxiliary_client_helpers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_agent_auxiliary_client_helpers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int aux__is_anthropic_compatible_host(const char *);
extern int aux__evict_cached_clients(const char *);
extern int aux__is_openrouter_client(const char *);
extern int aux__refresh_provider_credentials(const char *);
extern int aux__task_minimum_context_length(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_aux__is_anthropic_compatible_host(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (aux__is_anthropic_compatible_host(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__is_anthropic_compatible_host"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_aux__evict_cached_clients(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    (void)aux__evict_cached_clients(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__evict_cached_clients"));
    /* Python's _evict_cached_clients is void (returns None); the oracle
     * serializes None as ''. Emit '' — the eviction itself is side-effect
     * work, not part of the return contract. */
    json_set(o, "out", json_string("")); return o;
}

static json_t *emit_aux__is_openrouter_client(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (aux__is_openrouter_client(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__is_openrouter_client"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_aux__refresh_provider_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    bool v = (aux__refresh_provider_credentials(value) ? true : false);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__refresh_provider_credentials"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_aux__task_minimum_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)aux__task_minimum_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("aux__task_minimum_context_length"));
    /* Python returns Optional[int]; None serializes to '' in the oracle.
     * The C port signals None with -1 — emit '' for parity. */
    if (v < 0) json_set(o, "out", json_string(""));
    else json_set(o, "out", json_int(v)); return o;
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
        else if (strcmp(op, "aux__evict_cached_clients") == 0) o = emit_aux__evict_cached_clients(c);
        else if (strcmp(op, "aux__is_openrouter_client") == 0) o = emit_aux__is_openrouter_client(c);
        else if (strcmp(op, "aux__refresh_provider_credentials") == 0) o = emit_aux__refresh_provider_credentials(c);
        else if (strcmp(op, "aux__task_minimum_context_length") == 0) o = emit_aux__task_minimum_context_length(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
