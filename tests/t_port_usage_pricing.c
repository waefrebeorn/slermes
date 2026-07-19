/*
 * t_port_usage_pricing.c — faithful verification harness for
 * src/agent/usage_pricing.c (agent/usage_pricing.py).
 *
 * Reads a JSON array fixture from argv[1]; each element is one case with an
 * "op" discriminator. Runs the corresponding ported C helper and emits one
 * JSON line per case. The Python oracle (tests/sta_oracle_usage_pricing.py)
 * recomputes the SAME helpers from the LIVE agent/usage_pricing.py; the runner
 * diffs them byte-for-byte.
 *
 * Covered ops (the genuine gaps closed this session):
 *   normalize_usage        -> 3-way provider token normalization
 *   resolve_billing_route  -> nous/vertex/openai-codex routing (now live)
 *   format_token_count     -> compact token formatting
 *   bedrock_norm           -> cross-region Bedrock id normalization
 *   anthropic_norm         -> Anthropic model-name normalization
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "usage_pricing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static const char *mode_name(billing_mode_t m)
{
    switch (m) {
        case BILLING_DOCS_SNAPSHOT: return "official_docs_snapshot";
        case BILLING_MODELS_API:    return "official_models_api";
        case BILLING_SUB_INCLUDED:  return "subscription_included";
        default:                    return "unknown";
    }
}

static json_t *emit_normalize_usage(const json_t *c)
{
    const char *provider = json_get_str(c, "provider", "");
    const char *api_mode = json_get_str(c, "api_mode", "");
    json_t *usage = json_obj_get(c, "usage");
    usage_counts_t u = usage_pricing_normalize_usage(provider, api_mode, usage);

    json_t *o = json_new_object();
    json_set(o, "fn", json_string("normalize_usage"));
    json_set(o, "input_tokens", json_number(u.input_tokens));
    json_set(o, "output_tokens", json_number(u.output_tokens));
    json_set(o, "cache_read_tokens", json_number(u.cache_read_tokens));
    json_set(o, "cache_write_tokens", json_number(u.cache_write_tokens));
    json_set(o, "reasoning_tokens", json_number(u.reasoning_tokens));
    return o;
}

static json_t *emit_resolve_billing_route(const json_t *c)
{
    const char *model = json_get_str(c, "model", "");
    const char *provider = json_get_str(c, "provider", "");
    const char *base_url = json_get_str(c, "base_url", "");
    billing_route_t r = usage_pricing_resolve_billing_route(model, provider, base_url);

    json_t *o = json_new_object();
    json_set(o, "fn", json_string("resolve_billing_route"));
    json_set(o, "provider", json_string(r.provider));
    json_set(o, "model", json_string(r.model_name));
    json_set(o, "base_url", json_string(r.base_url));
    json_set(o, "billing_mode", json_string(mode_name(r.mode)));
    return o;
}

static json_t *emit_format_token_count(const json_t *c)
{
    long long value = (long long)json_get_num(c, "value", 0.0);
    const char *s = usage_pricing_format_token_count(value);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("format_token_count"));
    json_set(o, "value", json_number(value));
    json_set(o, "out", json_string(s));
    return o;
}

static json_t *emit_bedrock_norm(const json_t *c)
{
    const char *model = json_get_str(c, "model", "");
    char out[256];
    usage_pricing_normalize_bedrock_model(model, out, sizeof(out));
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("bedrock_norm"));
    json_set(o, "in", json_string(model));
    json_set(o, "out", json_string(out));
    return o;
}

static json_t *emit_anthropic_norm(const json_t *c)
{
    const char *model = json_get_str(c, "model", "");
    char out[256];
    usage_pricing_normalize_anthropic_model(model, out, sizeof(out));
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("anthropic_norm"));
    json_set(o, "in", json_string(model));
    json_set(o, "out", json_string(out));
    return o;
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
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (op[0] == '\0') { o = json_new_object(); json_set(o, "fn", json_string("UNKNOWN")); }
        else if (strcmp(op, "normalize_usage") == 0)        o = emit_normalize_usage(c);
        else if (strcmp(op, "resolve_billing_route") == 0)  o = emit_resolve_billing_route(c);
        else if (strcmp(op, "format_token_count") == 0)     o = emit_format_token_count(c);
        else if (strcmp(op, "bedrock_norm") == 0)           o = emit_bedrock_norm(c);
        else if (strcmp(op, "anthropic_norm") == 0)         o = emit_anthropic_norm(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }

        char *ser = json_serialize(o);
        printf("%s\n", ser);
        free(ser);
        json_free(o);
    }
    free(input);
    return 0;
}
