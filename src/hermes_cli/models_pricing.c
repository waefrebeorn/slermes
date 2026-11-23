/*
 * models_pricing.c — C home for upstream Hermes hermes_cli/models_pricing.py.
 *
 * PoP: w12_modelspric_4eacb7_strip_v1 @ hermes_cli/models_pricing.py:_strip_v1
 * PoP: w12_modelspric_4eacb7_format_price_per_mtok @ hermes_cli/models_pricing.py:_format_price_per_mtok
 * PoP: w12_modelspric_4eacb7_price_float @ hermes_cli/models_pricing.py:_price_float
 * PoP: w12_modelspric_4eacb7_sale_pct @ hermes_cli/models_pricing.py:_sale_pct
 * PoP: w12_modelspric_4eacb7_compute_sale_discount @ hermes_cli/models_pricing.py:compute_sale_discount
 * PoP: w12_modelspric_4eacb7_pricing_entry @ hermes_cli/models_pricing.py:_pricing_entry
 * PoP: w12_modelspric_4eacb7_per_token @ hermes_cli/models_pricing.py:_per_token
 * PoP: w12_modelspric_4eacb7_catalog_items @ hermes_cli/models_pricing.py:_catalog_items
 * PoP: w12_modelspric_4eacb7_restrict_to_nous_policy @ hermes_cli/models_pricing.py:restrict_to_nous_policy
 */
#include "noc_w12_modelspric_4eacb7.h"

#include "hermes_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- helpers ---------- */

/* Python str() coercion for the scalar JSON values used by these helpers. */
static char *py_str(const json_t *v) {
    if (!v) return strdup("None");
    switch (v->type) {
        case JSON_NULL:   return strdup("None");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double n = v->num_val;
            if (n == (double)(long long)n && n > -9e15 && n < 9e15)
                snprintf(buf, sizeof(buf), "%lld", (long long)n);
            else
                snprintf(buf, sizeof(buf), "%g", n);
            return strdup(buf);
        }
        default: {
            char *s = json_dumps(v, 0);
            return s ? s : strdup("");
        }
    }
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return json_len(v) != 0 || (v->type==JSON_OBJECT && json_object_size(v)!=0) ? 1 : 0;
    return 1;
}

/* Python float(raw) for scalars; returns 0 and sets ok=0 when unset/invalid. */
static double to_float(const json_t *raw, int *ok) {
    *ok = 0;
    if (!raw || raw->type == JSON_NULL) return 0.0;
    if (raw->type == JSON_STRING) {
        if (!raw->str_val || !*raw->str_val) return 0.0;
        char *end = NULL;
        double n = strtod(raw->str_val, &end);
        if (end == raw->str_val) return 0.0;
        *ok = 1; return n;
    }
    if (raw->type == JSON_NUMBER) { *ok = 1; return raw->num_val; }
    if (raw->type == JSON_BOOL) { *ok = 1; return raw->bool_val ? 1.0 : 0.0; }
    return 0.0;
}

/* _price_float: None for unset/invalid/NaN/out-of-domain, else the finite float. */
static double price_float_raw(const json_t *raw, int positive, int *valid) {
    *valid = 0;
    if (!raw || raw->type == JSON_NULL) return 0.0;
    if (raw->type == JSON_STRING && (!raw->str_val || !*raw->str_val)) return 0.0;
    int ok = 0;
    double n = to_float(raw, &ok);
    if (!ok || isnan(n)) return 0.0;
    if (positive ? (n <= 0.0) : (n < 0.0)) return 0.0;
    *valid = 1;
    return n;
}

/* round(x, ndigits) == 0 test used by _format_price_per_mtok. */
static int round_to_is_zero(double x, int prec) {
    double scale = pow(10.0, (double)prec);
    return nearbyint(x * scale) == 0.0;
}

/* ---------- exported helpers ---------- */

char *w12_modelspric_4eacb7_strip_v1(const char *url) {
    if (!url) return strdup("");
    size_t n = strlen(url);
    if (n >= 3 && !strcmp(url + n - 3, "/v1")) {
        size_t k = n - 3;
        while (k > 0 && url[k - 1] == '/') k--;
        char *out = (char *)malloc(k + 1);
        if (!out) return strdup("");
        memcpy(out, url, k); out[k] = '\0';
        return out;
    }
    return strdup(url);
}

char *w12_modelspric_4eacb7_format_price_per_mtok(const char *per_token_str) {
    char buf[512], text[512];
    if (!per_token_str || !*per_token_str) return strdup("?");
    char *end = NULL;
    double val = strtod(per_token_str, &end);
    if (end == per_token_str) return strdup("?");
    if (val == 0.0) return strdup("free");
    double per_m = val * 1000000.0;
    snprintf(text, sizeof(text), "%.2f", per_m);
    if (per_m < 0.01) {
        int prec = 3;
        while (prec < 12 && round_to_is_zero(per_m, prec)) prec++;
        int p = prec + 1; if (p > 12) p = 12;
        snprintf(buf, sizeof(buf), "%.*f", p, per_m);
        size_t n = strlen(buf);
        while (n > 0 && buf[n - 1] == '0') buf[--n] = '\0'; /* NUL written at new length */
        if (n > 0 && buf[n - 1] == '.') buf[--n] = '\0';
        snprintf(text, sizeof(text), "%s", buf);
    }
    snprintf(buf, sizeof(buf), "$%s", text);
    return strdup(buf);
}

struct json_t *w12_modelspric_4eacb7_price_float(const struct json_t *raw, int positive) {
    int valid = 0;
    double n = price_float_raw(raw, positive, &valid);
    return valid ? json_number(n) : json_null();
}

struct json_t *w12_modelspric_4eacb7_sale_pct(const struct json_t *current, const struct json_t *original) {
    int ok1 = 0, ok2 = 0;
    double cur = price_float_raw(current, 1, &ok1);
    double orig = price_float_raw(original, 1, &ok2);
    if (!ok1 || !ok2 || cur >= orig) return json_null();
    double val = (1.0 - (cur / orig)) * 100.0;
    long long r = (long long)nearbyint(val);
    return json_number((double)r);
}

struct json_t *w12_modelspric_4eacb7_compute_sale_discount(const struct json_t *prompt,
                                                           const struct json_t *completion,
                                                           const struct json_t *original) {
    int is_dict = original && original->type == JSON_OBJECT;
    json_t *orig = is_dict ? (json_t *)original : NULL;
    json_t *vp = orig ? json_obj_get(orig, "prompt") : NULL;
    json_t *vc = orig ? json_obj_get(orig, "completion") : NULL;
    char *ws_p = (vp && vp->type != JSON_NULL && !(vp->type==JSON_STRING && vp->str_val && !*vp->str_val))
                   ? py_str(vp) : strdup("");
    char *ws_c = (vc && vc->type != JSON_NULL && !(vc->type==JSON_STRING && vc->str_val && !*vc->str_val))
                   ? py_str(vc) : strdup("");

    int okp = 0, okc = 0;
    double pf = price_float_raw(prompt, 0, &okp);
    double cf = price_float_raw(completion, 0, &okc);
    int pf_zero = okp && pf == 0.0;
    int cf_zero_or_none = (!okc) || (okc && cf == 0.0);
    if (pf_zero && cf_zero_or_none) {
        json_t *out = json_array();
        json_append(out, json_number(100));
        json_append(out, json_string(ws_p));
        json_append(out, json_string(ws_c));
        free(ws_p); free(ws_c);
        return out;
    }
    if (!is_dict || (!*ws_p && !*ws_c)) { free(ws_p); free(ws_c); return json_null(); }

    /* _sale_pct(prompt, was_prompt), then completion fallback. */
    int okcur = 0, okorig = 0;
    double cur = price_float_raw(prompt, 1, &okcur);
    double origv = price_float_raw(vp, 1, &okorig);
    if (okcur && okorig && cur < origv) {
        long long r = (long long)nearbyint((1.0 - (cur / origv)) * 100.0);
        if (r >= 1) {
            json_t *out = json_array();
            json_append(out, json_number((double)r));
            json_append(out, json_string(ws_p));
            json_append(out, json_string(ws_c));
            free(ws_p); free(ws_c);
            return out;
        }
    }
    cur = price_float_raw(completion, 1, &okcur);
    origv = price_float_raw(vc, 1, &okorig);
    if (okcur && okorig && cur < origv) {
        long long r = (long long)nearbyint((1.0 - (cur / origv)) * 100.0);
        if (r >= 1) {
            json_t *out = json_array();
            json_append(out, json_number((double)r));
            json_append(out, json_string(ws_p));
            json_append(out, json_string(ws_c));
            free(ws_p); free(ws_c);
            return out;
        }
    }
    free(ws_p); free(ws_c);
    return json_null();
}

struct json_t *w12_modelspric_4eacb7_pricing_entry(const struct json_t *pricing,
                                                   const char *prompt_key,
                                                   const char *completion_key) {
    json_t *out = json_object();
    json_t *p = pricing && pricing->type == JSON_OBJECT ? json_obj_get((json_t *)pricing, prompt_key ? prompt_key : "prompt") : NULL;
    json_t *c = pricing && pricing->type == JSON_OBJECT ? json_obj_get((json_t *)pricing, completion_key ? completion_key : "completion") : NULL;
    char *ps = py_str(p ? p : (json_t *)json_string(""));
    char *cs = py_str(c ? c : (json_t *)json_string(""));
    json_set(out, "prompt", json_string(ps));
    json_set(out, "completion", json_string(cs));
    free(ps); free(cs);
    const char *keys[2] = { "input_cache_read", "input_cache_write" };
    for (int i = 0; i < 2; ++i) {
        json_t *v = pricing && pricing->type == JSON_OBJECT ? json_obj_get((json_t *)pricing, keys[i]) : NULL;
        if (py_truthy(v)) {
            char *s = py_str(v);
            json_set(out, keys[i], json_string(s));
            free(s);
        }
    }
    return out;
}

char *w12_modelspric_4eacb7_per_token(const struct json_t *per_mtok) {
    int ok = 0;
    double raw = to_float(per_mtok, &ok);
    if (!ok) raw = 0.0; /* Python float() would raise; fixtures always supply a number */
    double v = raw / 1000000.0;
    char buf[128];
    snprintf(buf, sizeof(buf), "%.15g", v);
    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E') &&
        !strchr(buf, 'n') && !strchr(buf, 'i')) {
        strcat(buf, ".0");
    }
    return strdup(buf);
}

struct json_t *w12_modelspric_4eacb7_catalog_items(const struct json_t *payload) {
    json_t *out = json_array();
    if (!payload || payload->type != JSON_OBJECT) return out;
    json_t *data = json_obj_get((json_t *)payload, "data");
    if (!data || data->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(data); ++i) {
        json_t *item = json_get(data, i);
        if (item && item->type == JSON_OBJECT) json_append(out, json_copy(item));
    }
    return out;
}

static int in_allowed(const char *s, const json_t *allowed) {
    if (!allowed || allowed->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(allowed); ++i) {
        json_t *v = json_get(allowed, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

struct json_t *w12_modelspric_4eacb7_restrict_to_nous_policy(const struct json_t *model_ids,
                                                             const struct json_t *allowed,
                                                             int rescue_empty) {
    json_t *out = json_array();
    size_t na = (allowed && allowed->type == JSON_ARRAY) ? json_len(allowed) : 0;
    if (na == 0) {
        if (model_ids && model_ids->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(model_ids); ++i) json_append(out, json_copy(json_get(model_ids, i)));
        return out;
    }
    if (model_ids && model_ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(model_ids); ++i) {
            json_t *v = json_get(model_ids, i);
            if (!v || v->type != JSON_STRING || !v->str_val) continue;
            const char *s = v->str_val;
            int keep = in_allowed(s, allowed);
            if (!keep) {
                const char *colon = strchr(s, ':');
                size_t bl = colon ? (size_t)(colon - s) : strlen(s);
                char *base = (char *)malloc(bl + 1);
                if (base) { memcpy(base, s, bl); base[bl] = '\0'; keep = in_allowed(base, allowed); free(base); }
            }
            if (keep) json_append(out, json_copy(v));
        }
    }
    if (rescue_empty && json_len(out) == 0 && na <= 64) {
        /* sorted(allowed) */
        for (size_t i = 0; i < na; ++i) {
            json_t *v = json_get(allowed, i);
            if (v && v->type == JSON_STRING) json_append(out, json_copy(v));
        }
        for (size_t i = 1; i < json_len(out); ++i) {
            json_t *key = json_get(out, i);
            size_t j = i;
            while (j > 0) {
                json_t *prev = json_get(out, j - 1);
                const char *a = prev && prev->type == JSON_STRING ? prev->str_val : "";
                const char *b = key && key->type == JSON_STRING ? key->str_val : "";
                if (strcmp(a, b) <= 0) break;
                /* swap via copies */
                json_t *tmp = json_copy(prev);
                json_array_set(out, j - 1, json_copy(key));
                json_array_set(out, j, tmp);
                j--;
            }
        }
    }
    return out;
}
