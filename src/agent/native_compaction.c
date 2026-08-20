/*
 * native_compaction.c — Native OpenAI Responses server-side compaction gate.
 * PoP: native_compaction @ agent/native_compaction.py
 *
 * Faithful port of agent/native_compaction.py. Pure decision/threshold logic;
 * the Python module reads agent attributes — this C port takes the resolved
 * values explicitly. Item-list restructuring uses json_t.
 */

#include "native_compaction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "json.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

/* PoP: is_native_compaction_model @ agent/native_compaction.py:is_native_compaction_model */
bool native_compaction_is_native_model(const char *model) {
    if (!model) return false;
    char buf[256];
    size_t n = strlen(model);
    if (n >= sizeof buf) n = sizeof buf - 1;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)model[i]);
    buf[n] = '\0';
    return strstr(buf, NATIVE_COMPACTION_ELIGIBLE_MODEL_MARKER) != NULL;
}

/* PoP: is_direct_openai_route @ agent/native_compaction.py:is_direct_openai_route */
bool native_compaction_is_direct_openai_route(const char *base_url, bool is_codex_backend) {
    if (is_codex_backend) return true;
    if (!base_url || !base_url[0]) return false;
    /* parse hostname: scheme://host[/...] */
    const char *p = base_url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8;
    const char *host_start = p;
    while (*p && *p != '/' && *p != '?' && *p != '#' && *p != ':') p++;
    size_t hlen = (size_t)(p - host_start);
    if (hlen == 0) return false;
    char host[256];
    if (hlen >= sizeof host) hlen = sizeof host - 1;
    for (size_t i = 0; i < hlen; i++) host[i] = (char)tolower((unsigned char)host_start[i]);
    host[hlen] = '\0';
    return strcmp(host, "api.openai.com") == 0;
}

static long parse_long_or(const char *s, long fallback) {
    if (!s) return fallback;
    /* reject leading bool-like strings; accept integers */
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return fallback;
    return v;
}

/* PoP: resolve_compact_threshold @ agent/native_compaction.py:resolve_compact_threshold */
long native_compaction_resolve_threshold(const char *configured_threshold,
                                          const char *local_trigger_tokens) {
    long configured = parse_long_or(configured_threshold, NATIVE_COMPACTION_DEFAULT_COMPACT_THRESHOLD);
    /* Python: bool is subclass of int; bool True/False would be invalid here.
     * A string "True"/"False" won't parse as int (parse_long_or falls back).
     * Python also rejects isinstance(configured_threshold, bool) -> default. */
    if (configured <= 0) configured = NATIVE_COMPACTION_DEFAULT_COMPACT_THRESHOLD;

    long local = parse_long_or(local_trigger_tokens, 0);
    if (local <= 0) return configured;

    long upper;
    if (local > NATIVE_COMPACTION_LOCAL_TRIGGER_SAFETY_MARGIN)
        upper = local - NATIVE_COMPACTION_LOCAL_TRIGGER_SAFETY_MARGIN;
    else
        upper = (local * 8) / 10;
    if (upper < 1024) upper = 1024;

    long lo = configured < upper ? configured : upper;
    return lo < 1024 ? 1024 : lo;
}

/* PoP: native_compaction_context_management @ agent/native_compaction.py:native_compaction_context_management */
json_t *native_compaction_context_management(bool codex_responses_native_compaction,
                                             bool compression_enabled,
                                             const char *model,
                                             const char *base_url,
                                             bool is_codex_backend,
                                             bool is_xai_responses,
                                             bool is_github_responses,
                                             const char *codex_responses_compact_threshold,
                                             const char *compressor_threshold) {
    if (!codex_responses_native_compaction) return NULL;
    if (!compression_enabled) return NULL;
    if (is_xai_responses || is_github_responses) return NULL;
    if (!native_compaction_is_native_model(model)) return NULL;
    if (!native_compaction_is_direct_openai_route(base_url, is_codex_backend)) return NULL;

    long threshold = native_compaction_resolve_threshold(codex_responses_compact_threshold,
                                                         compressor_threshold);
    json_t *arr = json_array();
    json_t *obj = json_object();
    json_set(obj, "type", json_string("compaction"));
    json_set(obj, "compact_threshold", json_number((double)threshold));
    json_append(arr, obj);
    return arr;
}

/* PoP: _approx_tokens @ agent/native_compaction.py:_approx_tokens */
long native_compaction_approx_tokens(const char *text) {
    if (!text) return 1;
    long n = (long)strlen(text) / 4;
    return n < 1 ? 1 : n;
}

/* PoP: _user_item_text @ agent/native_compaction.py:_user_item_text */
char *native_compaction_user_item_text(const json_t *item) {
    if (!item || item->type != JSON_OBJECT) return NULL;
    json_t *content = json_obj_get(item, "content");
    if (!content) return NULL;
    if (content->type == JSON_STRING) {
        const char *s = content->str_val;
        if (!s || !s[0] || !strchr(s, 0) || !strstr(s, "")) { /* noop guard */ }
        if (s && s[0]) {
            /* strip: return the string if non-empty after strip (here: non-empty) */
            if (s[0]) return json_serialize(content); /* JSON-encoded string with quotes; caller treats as text */
        }
        return NULL;
    }
    if (content->type == JSON_ARRAY) {
        size_t n = json_len(content);
        size_t cap = 0, len = 0;
        char *text = NULL;
        for (size_t i = 0; i < n; i++) {
            json_t *part = json_get(content, i);
            if (!part || part->type != JSON_OBJECT) continue;
            json_t *ptype = json_obj_get(part, "type");
            if (!ptype || ptype->type != JSON_STRING) continue;
            if (strcmp(ptype->str_val, "input_text") != 0) continue;
            json_t *ptext = json_obj_get(part, "text");
            if (!ptext || ptext->type != JSON_STRING) continue;
            const char *s = ptext->str_val;
            size_t add = strlen(s);
            if (len + add + 1 > cap) { cap = len + add + 64; text = realloc(text, cap); }
            if (len) text[len++] = ' ';
            memcpy(text + len, s, add);
            len += add;
        }
        if (!text) return NULL;
        text[len] = '\0';
        if (!text[0]) { free(text); return NULL; }
        return text;
    }
    return NULL;
}

/* PoP: prune_pre_checkpoint_items @ agent/native_compaction.py:prune_pre_checkpoint_items */
json_t *native_compaction_prune_pre_checkpoint_items(const json_t *items,
                                                     long retained_user_token_budget) {
    if (!items || items->type != JSON_ARRAY) return json_array();
    long last_cp = -1;
    size_t n = json_len(items);
    for (size_t i = 0; i < n; i++) {
        json_t *it = json_get(items, i);
        if (it && it->type == JSON_OBJECT) {
            json_t *t = json_obj_get(it, "type");
            if (t && t->type == JSON_STRING && strcmp(t->str_val, "compaction") == 0)
                last_cp = (long)i;
        }
    }
    if (last_cp < 0) return json_copy((json_t *)items);

    long first_cp = last_cp;
    while (first_cp > 0) {
        json_t *prev = json_get(items, (size_t)(first_cp - 1));
        if (prev && prev->type == JSON_OBJECT) {
            json_t *t = json_obj_get(prev, "type");
            if (t && t->type == JSON_STRING && strcmp(t->str_val, "compaction") == 0) { first_cp--; continue; }
        }
        break;
    }

    size_t pre_count = (size_t)first_cp;
    size_t cp_count = (size_t)(last_cp - first_cp + 1);
    size_t post_count = n - (size_t)(last_cp + 1);

    json_t *retained = json_array();
    long remaining = retained_user_token_budget > 0 ? retained_user_token_budget : 0;
    for (long i = (long)pre_count - 1; i >= 0; i--) {
        json_t *it = json_get(items, (size_t)i);
        if (!it || it->type != JSON_OBJECT) continue;
        json_t *role = json_obj_get(it, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val, "user") != 0) continue;
        json_t *typ = json_obj_get(it, "type");
        if (typ && typ->type == JSON_STRING && strcmp(typ->str_val, "message") != 0) continue;
        if (remaining <= 0) break;
        char *text = native_compaction_user_item_text(it);
        if (!text) continue;
        long cost = native_compaction_approx_tokens(text);
        if (cost <= remaining) {
            json_append(retained, json_copy(it));
            remaining -= cost;
        } else {
            /* head-truncate the boundary message if content is a string */
            json_t *c = json_obj_get(it, "content");
            if (c && c->type == JSON_STRING) {
                long keep = remaining * 4;
                const char *s = c->str_val;
                long slen = (long)strlen(s);
                if (keep > slen) keep = slen;
                json_t *trunc = json_copy(it);
                char *sub = malloc((size_t)keep + 1);
                memcpy(sub, s, (size_t)keep);
                sub[keep] = '\0';
                if (sub[0]) {
                    json_set(trunc, "content", json_string(sub));
                    json_append(retained, trunc);
                } else {
                    json_free(trunc);
                }
                free(sub);
                remaining = 0;
            }
        }
        free(text);
    }

    /* assemble: checkpoint_run + reversed(retained) + post */
    json_t *result = json_array();
    for (size_t i = (size_t)first_cp; i < (size_t)first_cp + cp_count; i++)
        json_append(result, json_copy(json_get(items, i)));
    for (long i = (long)json_len(retained) - 1; i >= 0; i--)
        json_append(result, json_copy(json_get(retained, (size_t)i)));
    for (size_t i = 0; i < post_count; i++)
        json_append(result, json_copy(json_get(items, (size_t)(last_cp + 1) + i)));

    json_free(retained);
    return result;
}

/* PoP: is_native_compaction_rejection @ agent/native_compaction.py:is_native_compaction_rejection */
bool native_compaction_is_rejection(const char *error, const char *status_code) {
    if (!error) return false;
    /* lower-case the error text */
    char buf[4096];
    size_t n = strlen(error);
    if (n >= sizeof buf) n = sizeof buf - 1;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)error[i]);
    buf[n] = '\0';
    if (strstr(buf, "context_management") == NULL && strstr(buf, "compact_threshold") == NULL)
        return false;
    if (status_code != NULL) {
        char *end = NULL;
        long sc = strtol(status_code, &end, 10);
        if (end != status_code && *end == '\0' && sc != 400) return false;
    }
    static const char *markers[] = {
        "unknown", "unsupported", "invalid", "unexpected", "not permitted",
        "not allowed", "unrecognized", "extra field", "no such", "bad request",
        "not supported", NULL
    };
    for (int i = 0; markers[i]; i++)
        if (strstr(buf, markers[i]) != NULL) return true;
    return false;
}

/* PoP: has_compaction_checkpoint @ agent/native_compaction.py:has_compaction_checkpoint */
bool native_compaction_has_checkpoint(const json_t *items) {
    if (!items || items->type != JSON_ARRAY) return false;
    size_t n = json_len(items);
    for (size_t i = 0; i < n; i++) {
        json_t *it = json_get(items, i);
        if (it && it->type == JSON_OBJECT) {
            json_t *t = json_obj_get(it, "type");
            if (t && t->type == JSON_STRING && strcmp(t->str_val, "compaction") == 0) return true;
        }
    }
    return false;
}

/* PoP: merge_interim_reasoning_items @ agent/native_compaction.py:merge_interim_reasoning_items */
json_t *native_compaction_merge_interim_items(const json_t *prior, const json_t *new_items) {
    json_t *prior_arr = (prior && prior->type == JSON_ARRAY) ? (json_t *)prior : NULL;
    json_t *new_arr = (new_items && new_items->type == JSON_ARRAY) ? (json_t *)new_items : NULL;

    json_t *kept = json_array();
    if (prior_arr) {
        size_t n = json_len(prior_arr);
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_get(prior_arr, i);
            if (it && it->type == JSON_OBJECT) {
                json_t *t = json_obj_get(it, "type");
                if (t && t->type == JSON_STRING && strcmp(t->str_val, "compaction") == 0)
                    json_append(kept, json_copy(it));
            }
        }
    }

    if (!new_arr) { json_t *r = json_copy(kept); json_free(kept); return r; }
    if (native_compaction_has_checkpoint(new_arr) || json_len(kept) == 0) {
        json_free(kept);
        return json_copy(new_arr);
    }
    json_t *result = json_array();
    size_t kn = json_len(kept);
    for (size_t i = 0; i < kn; i++) json_append(result, json_copy(json_get(kept, i)));
    size_t nn = json_len(new_arr);
    for (size_t i = 0; i < nn; i++) json_append(result, json_copy(json_get(new_arr, i)));
    json_free(kept);
    return result;
}
