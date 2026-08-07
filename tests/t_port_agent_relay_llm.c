/* Oracle harness for src/agent/port_agent_relay_llm.c (agent/relay_llm.py pure
 * helpers). Reads a JSON array fixture from argv[1]; each element is
 * {"op":<cfunc>, ...args...}. Emits one JSON result per line.
 *
 * Verified C -> Python mapping:
 *   relay_llm_jsonable        -> relay_llm._jsonable
 *   relay_llm_namespace_get   -> relay_llm._namespace (dict-key lookup)
 *   relay_llm_json_equal      -> relay_llm._json_equal
 *   relay_llm_is_cancellation -> relay_llm._is_cancellation
 *   relay_llm_codec           -> relay_llm._codec
 *   relay_llm_provider_request_body -> relay_llm._provider_request_body
 *   relay_llm_relay_request_body  -> relay_llm._relay_request_body
 *   relay_llm_provider_request  -> relay_llm._provider_request
 *   anthropic_stream_accumulator_observe -> AnthropicStreamAccumulator.observe
 *   anthropic_stream_accumulator_finalize -> AnthropicStreamAccumulator.finalize
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "port_agent_relay_llm.h"

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *get(const json_t *obj, const char *key){
    json_t *v = json_obj_get(obj, key);
    return v;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL;
    json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    free(input);
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be JSON array\n"); json_free(root); return 2; }

    for (size_t i = 0; i < root->c.count; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *out = json_object();
        json_set(out, "op", json_string(op));

        if (strcmp(op, "jsonable") == 0) {
            json_t *in = get(c, "value");
            json_t *r = relay_llm_jsonable(in);
            json_set(out, "value", r ? json_copy(r) : json_null());
            json_free(r);
        } else if (strcmp(op, "namespace_get") == 0) {
            json_t *in = get(c, "value");
            const char *key = json_get_str(c, "key", "");
            relay_llm_namespace_t *ns = relay_llm_namespace(in);
            const json_t *v = ns ? relay_llm_namespace_get(ns, key) : NULL;
            json_set(out, "value", v ? json_copy(v) : json_null());
            relay_llm_namespace_free(ns);
        } else if (strcmp(op, "json_equal") == 0) {
            bool r = relay_llm_json_equal(get(c, "left"), get(c, "right"));
            json_set(out, "value", json_bool(r));
        } else if (strcmp(op, "is_cancellation") == 0) {
            bool r = relay_llm_is_cancellation(json_get_str(c, "error_kind", ""));
            json_set(out, "value", json_bool(r));
        } else if (strcmp(op, "codec") == 0) {
            char *r = relay_llm_codec(get(c, "metadata"));
            json_set(out, "value", r ? json_string(r) : json_null());
            free(r);
        } else if (strcmp(op, "provider_request_body") == 0) {
            json_t *r = relay_llm_provider_request_body(get(c, "content"), get(c, "metadata"));
            json_set(out, "value", r ? json_copy(r) : json_null());
            json_free(r);
        } else if (strcmp(op, "relay_request_body") == 0) {
            json_t *r = relay_llm_relay_request_body(get(c, "request"), get(c, "metadata"));
            json_set(out, "value", r ? json_copy(r) : json_null());
            json_free(r);
        } else if (strcmp(op, "provider_request") == 0) {
            json_t *r = relay_llm_provider_request(
                get(c, "original"), get(c, "next_request"),
                get(c, "relay_request_body"), get(c, "codec_baseline_body"),
                get(c, "metadata"));
            json_set(out, "value", r ? json_copy(r) : json_null());
            json_free(r);
        } else if (strcmp(op, "accumulator_observe") == 0) {
            anthropic_stream_accumulator_t *acc = anthropic_stream_accumulator_new();
            size_t n = c->c.count;
            for (size_t j = 0; j < n; j++){
                if (strcmp(c->c.keys[j], "events") == 0 && c->c.items[j]->type == JSON_ARRAY){
                    json_t *arr = c->c.items[j];
                    for (size_t k = 0; k < arr->c.count; k++)
                        anthropic_stream_accumulator_observe(acc, json_get(arr, k));
                }
            }
            json_set(out, "value", json_string("{}"));
            anthropic_stream_accumulator_free(acc);
        } else if (strcmp(op, "accumulator_finalize") == 0) {
            anthropic_stream_accumulator_t *acc = anthropic_stream_accumulator_new();
            json_t *events = get(c, "events");
            if (events && events->type == JSON_ARRAY)
                for (size_t k = 0; k < events->c.count; k++)
                    anthropic_stream_accumulator_observe(acc, json_get(events, k));
            json_t *r = anthropic_stream_accumulator_finalize(acc);
            json_set(out, "value", r ? json_copy(r) : json_null());
            json_free(r);
            anthropic_stream_accumulator_free(acc);
        } else {
            json_set(out, "error", json_string("unknown op"));
        }

        char *ser = json_serialize(out);
        printf("%s\n", ser);
        free(ser);
        json_free(out);
    }
    json_free(root);
    return 0;
}
