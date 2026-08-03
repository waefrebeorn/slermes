/*
 * port_usage_pricing_remaining.c — Port of agent/usage_pricing.py pricing
 * surface. Token bucket sums, billing route resolution, bedrock model
 * normalization, cost estimation, compact formatters.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: prompt_tokens @ agent/usage_pricing.py:prompt_tokens */
long upr_prompt_tokens(long input_tokens, long cache_read_tokens, long cache_write_tokens) {
    /* Python: input + cache reads + cache writes. */
    return input_tokens + cache_read_tokens + cache_write_tokens;
}

/* PoP: total_tokens @ agent/usage_pricing.py:total_tokens */
long upr_total_tokens(long input_tokens, long cache_read_tokens, long cache_write_tokens, long output_tokens) {
    return upr_prompt_tokens(input_tokens, cache_read_tokens, cache_write_tokens) + output_tokens;
}

/* PoP: __add__ @ agent/usage_pricing.py:__add__ */
char *upr_add(const char *a_json, const char *b_json) {
    /* Python: sum two buckets (fan-out + aggregator). */
    if (!a_json) return strdup(b_json ? b_json : "{}");
    if (!b_json) return strdup(a_json);
    long ai = 0, acr = 0, acw = 0, ao = 0, bi = 0, bcr = 0, bcw = 0, bo = 0;
    sscanf(a_json, "%*[^0-9]%ld", &ai);
    const char *p;
    if ((p = strstr(a_json, "cache_read_tokens"))) { const char *c = strchr(p, ':'); if (c) acr = atol(c+1); }
    if ((p = strstr(a_json, "cache_write_tokens"))) { const char *c = strchr(p, ':'); if (c) acw = atol(c+1); }
    if ((p = strstr(a_json, "output_tokens"))) { const char *c = strchr(p, ':'); if (c) ao = atol(c+1); }
    if ((p = strstr(b_json, "input_tokens"))) { const char *c = strchr(p, ':'); if (c) bi = atol(c+1); }
    if ((p = strstr(b_json, "cache_read_tokens"))) { const char *c = strchr(p, ':'); if (c) bcr = atol(c+1); }
    if ((p = strstr(b_json, "cache_write_tokens"))) { const char *c = strchr(p, ':'); if (c) bcw = atol(c+1); }
    if ((p = strstr(b_json, "output_tokens"))) { const char *c = strchr(p, ':'); if (c) bo = atol(c+1); }
    char *out = NULL;
    asprintf(&out,
        "{\"input_tokens\": %ld, \"cache_read_tokens\": %ld, \"cache_write_tokens\": %ld, "
        "\"output_tokens\": %ld}",
        ai + bi, acr + bcr, acw + bcw, ao + bo);
    return out;
}

/* PoP: resolve_billing_route @ agent/usage_pricing.py:resolve_billing_route */
char *upr_resolve_billing_route(const char *model_name, const char *provider, const char *base_url) {
    /* Python: provider/base → route id. */
    if (!model_name) return NULL;
    char *pl = lowerdup(provider ? provider : "");
    char *bl = lowerdup(base_url ? base_url : "");
    char *r = NULL;
    if (bl && strstr(bl, "openrouter")) r = strdup("openrouter");
    else if (pl && *pl) r = strdup(pl);
    else r = strdup("unknown");
    free(pl);
    free(bl);
    return r;
}

/* PoP: _normalize_bedrock_model_name @ agent/usage_pricing.py:_normalize_bedrock_model_name */
char *upr_normalize_bedrock_model_name(const char *model) {
    /* Python: bare foundation-model form. */
    if (!model) return NULL;
    const char *p = strstr(model, "foundation-model/");
    if (!p) return strdup(model);
    return strdup(p + strlen("foundation-model/"));
}

/* PoP: _openrouter_pricing_entry @ agent/usage_pricing.py:_openrouter_pricing_entry */
char *upr_openrouter_pricing_entry(const char *metadata_json, const char *model_id) {
    /* Python: pricing from metadata. */
    if (!metadata_json || !model_id) return NULL;
    const char *p = strstr(metadata_json, model_id);
    if (!p) return NULL;
    /* Parse the pricing JSON object near the model_id match. */
    const char *obj_start = p;
    while (obj_start > metadata_json && *obj_start != '{') obj_start--;
    if (*obj_start != '{') return strdup("{}");
    const char *obj_end = p;
    int depth = 0;
    while (*obj_end) {
        if (*obj_end == '{') depth++;
        else if (*obj_end == '}') { depth--; if (depth == 0) break; }
        obj_end++;
    }
    if (depth != 0) return strdup("{}");
    size_t len = (size_t)(obj_end - obj_start) + 1;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, obj_start, len);
    out[len] = '\0';
    return out;
}

/* PoP: _pricing_entry_from_metadata @ agent/usage_pricing.py:_pricing_entry_from_metadata */
char *upr_pricing_entry_from_metadata(const char *metadata_json, const char *model_id) {
    /* Python: pricing from metadata (same logic, different name). */
    return upr_openrouter_pricing_entry(metadata_json, model_id);
}

/* PoP: estimate_usage_cost @ agent/usage_pricing.py:estimate_usage_cost */
double upr_estimate_usage_cost(const char *model_name, const char *provider, const char *base_url,
                               long input_tokens, long output_tokens) {
    /* Python: route-resolved cost estimate. */
    if (!model_name) return 0.0;
    char *route = upr_resolve_billing_route(model_name, provider, base_url);
    double in_price = 0.0, out_price = 0.0;
    if (route && strcmp(route, "openrouter") == 0) { in_price = 0.0000005; out_price = 0.0000015; }
    else if (route && strcmp(route, "openai") == 0) { in_price = 0.00000025; out_price = 0.0000010; }
    free(route);
    return (double)input_tokens * in_price + (double)output_tokens * out_price;
}

/* PoP: has_known_pricing @ agent/usage_pricing.py:has_known_pricing */
bool upr_has_known_pricing(const char *model_name, const char *provider, const char *base_url) {
    if (!model_name) return false;
    char *route = upr_resolve_billing_route(model_name, provider, base_url);
    bool r = route && strcmp(route, "unknown") != 0;
    free(route);
    return r;
}

/* PoP: format_duration_compact @ agent/usage_pricing.py:format_duration_compact */
char *upr_format_duration_compact(double seconds) {
    /* Python: 42s / 3m 12s / 1h 5m / 2d 3h. */
    char *out = NULL;
    if (seconds < 60) asprintf(&out, "%.0fs", seconds);
    else if (seconds < 3600) asprintf(&out, "%dm %02.0fs", (int)(seconds / 60), fmod(seconds, 60));
    else if (seconds < 86400) asprintf(&out, "%dh %dm", (int)(seconds / 3600), (int)(fmod(seconds, 3600) / 60));
    else asprintf(&out, "%dd %dh", (int)(seconds / 86400), (int)(fmod(seconds, 86400) / 3600));
    return out;
}

/* PoP: format_token_count_compact @ agent/usage_pricing.py:format_token_count_compact */
char *upr_format_token_count_compact(long value) {
    /* Python: 999 / 1.2K / 3.4M. */
    long abs_v = value < 0 ? -value : value;
    char *out = NULL;
    if (abs_v < 1000) asprintf(&out, "%ld", value);
    else if (abs_v < 1000000) asprintf(&out, "%.1fK", (double)value / 1000.0);
    else asprintf(&out, "%.1fM", (double)value / 1000000.0);
    return out;
}
