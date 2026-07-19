/*
 * t_port_provider_custom.c — faithful verification harness for the custom-provider
 * request-shaping helpers in src/agent/provider_custom.c
 * (port of agent/agent_init.py:_custom_provider_extra_body_for_agent /
 *  _merge_custom_provider_extra_body / _custom_provider_model_matches).
 *
 * Reads a JSON array fixture from argv[1]; each element is an object:
 *   {provider, model, base_url, custom_providers, existing_extra_body?}
 * Emits one JSON line per element:
 *   {"case":i,"extra_body":<resolved>,"merged":<merge result>}
 * The Python oracle (tests/sta_oracle_provider_custom.py) recomputes from the
 * LIVE agent/agent_init.py; the runner diffs them byte-for-byte.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations — defined in src/agent/provider_custom.c (port of
 * agent/agent_init.py). Avoid pulling provider.h (which drags in hermes_agent.h
 * -> libdb) so the oracle harness links with the minimal include set. */
char *custom_normalized_base_url(const char *value);
bool custom_provider_model_matches(const char *agent_model, const json_t *entry);
json_t *custom_provider_extra_body_for_agent(const char *provider,
                                           const char *model,
                                           const char *base_url,
                                           const json_t *custom_providers);
json_t *custom_merge_extra_body(const char *provider, const char *model,
                               const char *base_url,
                               const json_t *custom_providers,
                               const json_t *existing_extra_body);

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

static const char *opt_str(const json_t *o, const char *k)
{
    const json_t *v = json_object_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

static json_t *opt_obj(const json_t *o, const char *k)
{
    const json_t *v = json_object_get(o, k);
    return (v && (v->type == JSON_OBJECT || v->type == JSON_ARRAY)) ? (json_t *)v : NULL;
}

static void emit(const char *key, const json_t *val, json_t *out)
{
    if (val) json_set(out, key, json_copy((json_t *)val));
    else json_set(out, key, json_null());
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char *err = NULL;
    json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }

    int n = json_array_size(root);
    for (int i = 0; i < n; i++) {
        json_t *c = json_get(root, i);
        const char *provider = opt_str(c, "provider");
        const char *model = opt_str(c, "model");
        const char *base_url = opt_str(c, "base_url");
        const json_t *cps = opt_obj(c, "custom_providers");
        const json_t *existing = opt_obj(c, "existing_extra_body");

        json_t *eb = custom_provider_extra_body_for_agent(provider, model, base_url, cps);
        json_t *merged = custom_merge_extra_body(provider, model, base_url, cps, existing);

        json_t *o = json_new_object();
        json_set(o, "case", json_int(i));
        emit("extra_body", eb, o);
        emit("merged", merged, o);

        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
        if (eb) json_free(eb);
        if (merged) json_free(merged);
    }
    free(input);
    return 0;
}
