/*
 * image_token_cost.c — C port of upstream Hermes agent/image_token_cost.py.
 *
 * PoP: w8_imagetoken_c457d3_key @ agent/image_token_cost.py:_key
 * PoP: w8_imagetoken_c457d3_learned @ agent/image_token_cost.py:learned_image_token_cost
 * PoP: w8_imagetoken_c457d3_current @ agent/image_token_cost.py:current_image_token_cost
 * PoP: w8_imagetoken_c457d3_calibrate @ agent/image_token_cost.py:calibrate_from_usage
 *
 * Constants mirrored from the Python module: DEFAULT_IMAGE_TOKEN_COST=1500,
 * _MIN_PLAUSIBLE=64, _MAX_PLAUSIBLE=32768, _EMA_ALPHA=0.5.
 * base_url_hostname / count_images / anchored_context_tokens / _table are
 * injected from the fixture (documented seams).
 */
#include "noc_w8_imagetoken_c457d3.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_COST 1500LL
#define MIN_PLAUSIBLE 64LL
#define MAX_PLAUSIBLE 32768LL
#define EMA_ALPHA 0.5

/* Python `value or ''` for a string-typed operand. */
static const char *str_or_empty(const json_t *v) {
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) return v->str_val;
    return "";
}

/* Injected base_url_hostname seam: hosts[base_url] or ''. */
static const char *hostname(const json_t *hosts, const char *url) {
    const json_t *h = hosts ? json_obj_get(hosts, url) : NULL;
    if (h && h->type == JSON_STRING && h->str_val) return h->str_val;
    return "";
}

/* f"{model or ''}@{base_url_hostname(base_url or '') or ''}" */
static char *build_key(const json_t *model, const json_t *base_url, const json_t *hosts) {
    const char *m = str_or_empty(model);
    const char *b = str_or_empty(base_url);
    const char *hn = hostname(hosts, b);
    size_t n = strlen(m) + 1 + strlen(hn) + 1;
    char *buf = (char *)malloc(n);
    if (!buf) return NULL;
    snprintf(buf, n, "%s@%s", m, hn);
    return buf;
}

static long long table_get(const json_t *table, const char *key, long long def) {
    const json_t *v = table ? json_obj_get(table, key) : NULL;
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return def;
}

json_t *w8_imagetoken_c457d3_key(const json_t *model, const json_t *base_url,
                                 const struct json_t *hosts) {
    char *k = build_key(model, base_url, hosts);
    json_t *r = json_string(k ? k : "");
    free(k);
    return r;
}

json_t *w8_imagetoken_c457d3_learned(const json_t *model, const json_t *base_url,
                                     const struct json_t *hosts, const struct json_t *table) {
    char *k = build_key(model, base_url, hosts);
    long long v = table_get(table, k ? k : "", DEFAULT_COST);
    free(k);
    return json_number((double)v);
}

json_t *w8_imagetoken_c457d3_current(const struct json_t *bound) {
    if (bound && bound->type != JSON_NULL) {
        if (bound->type == JSON_NUMBER) return json_number((double)(long long)bound->num_val);
        if (bound->type == JSON_BOOL) return json_number(bound->bool_val ? 1.0 : 0.0);
    }
    return json_number((double)DEFAULT_COST);
}

/* Python int(x or 0); returns 0 on ValueError/TypeError (signalled by ret code). */
static int to_int_or_null(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) { *out = 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) { *out = 0; return 1; }
        char *end = NULL;
        long long n = strtoll(p, &end, 10);
        if (end == p) return 0;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end != '\0') return 0;
        *out = n;
        return 1;
    }
    return 0;
}

/* floor division for positive denominator */
static long long floor_div(long long a, long long b) {
    long long q = a / b;
    if (a % b != 0 && a < 0) q--;
    return q;
}

static int is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }

json_t *w8_imagetoken_c457d3_calibrate(const struct json_t *args) {
    if (!is_dict(args)) return json_null();
    const json_t *anchor = json_obj_get(args, "anchor");
    const json_t *messages = json_obj_get(args, "messages");
    const json_t *prompt_tokens = json_obj_get(args, "prompt_tokens");
    const json_t *text_only_v = json_obj_get(args, "text_only");
    const json_t *hosts = json_obj_get(args, "hosts");
    const json_t *model = json_obj_get(args, "model");
    const json_t *base_url = json_obj_get(args, "base_url");
    const json_t *table = json_obj_get(args, "table");

    long long real = 0;
    if (!to_int_or_null(prompt_tokens, &real)) return json_null();
    if (real <= 0 || !is_dict(anchor) || !messages || messages->type != JSON_ARRAY)
        return json_null();

    long long base_count = 0;
    const json_t *bc = json_obj_get(anchor, "base_count");
    if (!to_int_or_null(bc, &base_count)) base_count = 0;
    if (base_count < 0) base_count = 0;
    size_t start = (size_t)base_count;

    /* delta = messages[base_count:] ; drop leading assistant row if present */
    size_t count = messages->c.count;
    if (start > count) start = count;
    size_t dstart = start;
    if (dstart < count) {
        json_t *first = messages->c.items[dstart];
        if (is_dict(first)) {
            const json_t *role = json_obj_get(first, "role");
            if (role && role->type == JSON_STRING && role->str_val &&
                !strcmp(role->str_val, "assistant"))
                dstart++;
        }
    }

    /* n_images = sum(int(m.get("_images",0)) for dict m in delta) */
    long long n_images = 0;
    for (size_t i = dstart; i < count; i++) {
        json_t *m = messages->c.items[i];
        if (!is_dict(m)) continue;
        const json_t *iv = json_obj_get(m, "_images");
        long long c = 0;
        if (iv && iv->type == JSON_NUMBER) c = (long long)iv->num_val;
        n_images += c;
    }
    if (n_images <= 0) return json_null();

    long long text_only = 0;
    if (!to_int_or_null(text_only_v, &text_only)) text_only = 0;
    long long per_image = floor_div(real - text_only, n_images);
    if (per_image < MIN_PLAUSIBLE || per_image > MAX_PLAUSIBLE) return json_null();

    char *k = build_key(model, base_url, hosts);
    const json_t *prior_v = table ? json_obj_get(table, k ? k : "") : NULL;
    int has_prior = prior_v && prior_v->type == JSON_NUMBER;
    long long learned = per_image;
    if (has_prior) {
        double pr = prior_v->num_val;
        double pe = (double)per_image;
        learned = (long long)(pr + EMA_ALPHA * (pe - pr));
    }
    free(k);
    return json_number((double)learned);
}
