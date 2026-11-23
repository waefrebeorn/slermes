/*
 * gemini_long_decode.c — C home for upstream Hermes agent/gemini_long_decode.py.
 *
 * PoP: w10_geminilong_ef98e5_long_decode_max_output_tokens @ agent/gemini_long_decode.py:long_decode_max_output_tokens
 * PoP: w10_geminilong_ef98e5_long_decode_hops @ agent/gemini_long_decode.py:long_decode_hops
 * PoP: w10_geminilong_ef98e5_long_decode_stale_multiplier @ agent/gemini_long_decode.py:long_decode_stale_multiplier
 * PoP: w10_geminilong_ef98e5_continuation_token @ agent/gemini_long_decode.py:continuation_token
 * PoP: w10_geminilong_ef98e5_merge_hop_usage @ agent/gemini_long_decode.py:merge_hop_usage
 * PoP: w10_geminilong_ef98e5_merge_hop_responses @ agent/gemini_long_decode.py:merge_hop_responses
 */
#include "noc_w10_geminilong_ef98e5.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOP_TOKENS 32768LL

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] != '\0';
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return 0;
}

static long long int_cast(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return 0;
}

/* _candidate(payload): first element of `candidates` when it is an object. */
static const json_t *candidate(const json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return NULL;
    const json_t *c = json_obj_get(payload, "candidates");
    if (!c || c->type != JSON_ARRAY || c->c.count == 0) return NULL;
    const json_t *f = c->c.items[0];
    return (f && f->type == JSON_OBJECT) ? f : NULL;
}

long long w10_geminilong_ef98e5_long_decode_max_output_tokens(const char *model, const json_t *cfg) {
    const char *m = model ? model : "";
    while (*m && isspace((unsigned char)*m)) m++;
    size_t len = strlen(m);
    while (len > 0 && isspace((unsigned char)m[len - 1])) len--;
    char name[512];
    if (len >= sizeof(name)) len = sizeof(name) - 1;
    for (size_t i = 0; i < len; i++) name[i] = (char)tolower((unsigned char)m[i]);
    name[len] = '\0';
    const char *nm = name;
    if (strncmp(nm, "google/", 7) == 0) nm += 7;
    if (strncmp(nm, "gemini-4", 8) != 0) return -1;
    if (!cfg || cfg->type != JSON_OBJECT) return -1;
    const json_t *en = json_obj_get(cfg, "enabled");
    /* Python `is False`: only the JSON boolean false disables. */
    if (en && en->type == JSON_BOOL && en->bool_val == false) return -1;

    const json_t *ef = json_obj_get(cfg, "effort");
    char effort[256];
    if (!truthy(ef)) {
        effort[0] = '\0';
    } else if (ef->type == JSON_STRING) {
        const char *s = ef->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t el = strlen(s);
        while (el > 0 && isspace((unsigned char)s[el - 1])) el--;
        if (el >= sizeof(effort)) el = sizeof(effort) - 1;
        for (size_t i = 0; i < el; i++) effort[i] = (char)tolower((unsigned char)s[i]);
        effort[el] = '\0';
    } else if (ef->type == JSON_BOOL) {
        strcpy(effort, ef->bool_val ? "true" : "false");
    } else {
        snprintf(effort, sizeof(effort), "%lld", int_cast(ef));
    }
    if (!strcmp(effort, "xhigh")) return 262144;
    if (!strcmp(effort, "max") || !strcmp(effort, "ultra")) return 1048576;
    return -1;
}

long long w10_geminilong_ef98e5_long_decode_hops(const json_t *v) {
    long long budget = 0;
    if (v && v->type == JSON_NUMBER) budget = (long long)v->num_val;
    long long h = (budget + HOP_TOKENS - 1) / HOP_TOKENS; /* ceil for non-negative budgets */
    return h < 1 ? 1 : h;
}

long long w10_geminilong_ef98e5_long_decode_stale_multiplier(const json_t *payload) {
    const json_t *extra = (payload && payload->type == JSON_OBJECT)
                          ? json_obj_get(payload, "extra_body") : NULL;
    if (!extra || extra->type != JSON_OBJECT) return 1;
    return w10_geminilong_ef98e5_long_decode_hops(json_obj_get(extra, "max_output_tokens"));
}

char *w10_geminilong_ef98e5_continuation_token(const json_t *payload) {
    const json_t *c = candidate(payload);
    char fr[64] = {0};
    if (c) {
        const json_t *f = json_obj_get(c, "finishReason");
        if (f && f->type == JSON_STRING) {
            size_t n = strlen(f->str_val);
            if (n >= sizeof(fr)) n = sizeof(fr) - 1;
            for (size_t i = 0; i < n; i++) fr[i] = (char)toupper((unsigned char)f->str_val[i]);
            fr[n] = '\0';
        }
    }
    if (strcmp(fr, "CONTINUATION")) return NULL;
    const json_t *t = c ? json_obj_get(c, "continuationToken") : NULL;
    if (t && t->type == JSON_STRING && t->str_val && t->str_val[0]) return xstrdup(t->str_val);
    return NULL;
}

static const char *const USAGE_KEYS[] = {
    "promptTokenCount", "cachedContentTokenCount", "candidatesTokenCount",
    "thoughtsTokenCount", "toolUsePromptTokenCount", "totalTokenCount"
};

json_t *w10_geminilong_ef98e5_merge_hop_usage(const json_t *hops) {
    json_t *merged = json_object();
    size_t n = (hops && hops->type == JSON_ARRAY) ? hops->c.count : 0;
    for (size_t k = 0; k < sizeof(USAGE_KEYS) / sizeof(USAGE_KEYS[0]); k++) {
        const char *key = USAGE_KEYS[k];
        int present = 0;
        for (size_t i = 0; i < n; i++)
            if (json_has(hops->c.items[i], key)) { present = 1; break; }
        if (!present) continue;
        long long sum = 0;
        for (size_t i = 0; i < n; i++) sum += int_cast(json_obj_get(hops->c.items[i], key));
        json_set(merged, key, json_number((double)sum));
    }
    const json_t *first = (n > 0) ? hops->c.items[0] : NULL;
    json_set(merged, "initialPromptTokenCount",
             json_number((double)(first ? int_cast(json_obj_get(first, "promptTokenCount")) : 0)));
    json_set(merged, "initialCachedContentTokenCount",
             json_number((double)(first ? int_cast(json_obj_get(first, "cachedContentTokenCount")) : 0)));
    return merged;
}

static void copy_object_keys(json_t *dst, const json_t *src) {
    if (!src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < json_object_size(src); i++) {
        const char *k = json_object_get_key_at(src, i);
        if (k) json_set(dst, k, json_copy(json_object_get_at(src, i)));
    }
}

json_t *w10_geminilong_ef98e5_merge_hop_responses(const json_t *hops) {
    size_t n = (hops && hops->type == JSON_ARRAY) ? hops->c.count : 0;
    json_t *parts = json_array();
    for (size_t i = 0; i < n; i++) {
        const json_t *c = candidate(hops->c.items[i]);
        const json_t *content = c ? json_obj_get(c, "content") : NULL;
        if (!content || content->type != JSON_OBJECT) continue;
        const json_t *ps = json_obj_get(content, "parts");
        if (!ps || ps->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < ps->c.count; j++) json_append(parts, json_copy(ps->c.items[j]));
    }
    const json_t *last = (n > 0) ? candidate(hops->c.items[n - 1]) : NULL;
    json_t *cand = json_object();
    copy_object_keys(cand, last);
    json_t *content2 = json_object();
    const json_t *lastcontent = last ? json_obj_get(last, "content") : NULL;
    copy_object_keys(content2, (lastcontent && lastcontent->type == JSON_OBJECT) ? lastcontent : NULL);
    json_set(content2, "role", json_string("model"));
    json_set(content2, "parts", json_copy(parts));
    json_set(cand, "content", content2);

    json_t *umlist = json_array();
    for (size_t i = 0; i < n; i++) {
        const json_t *um = json_obj_get(hops->c.items[i], "usageMetadata");
        if (um && um->type == JSON_OBJECT) json_append(umlist, json_copy(um));
        else json_append(umlist, json_object());
    }
    json_t *usage = w10_geminilong_ef98e5_merge_hop_usage(umlist);
    json_free(umlist);

    json_t *out = json_object();
    const json_t *lasthop = (n > 0) ? hops->c.items[n - 1] : NULL;
    copy_object_keys(out, lasthop);
    json_t *cl = json_array();
    json_append(cl, cand);
    json_set(out, "candidates", cl);
    json_set(out, "usageMetadata", usage);
    json_free(parts);
    return out;
}
