#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/clarify_gateway.py: 4 missing functions ── */

/* PoP: label_matches @ tools/clarify_gateway.py:label_matches */
int label_matches(void) { return 0; }

/* ── tail_revive4 port (clarify_gateway) ───────────────────────────────────
 * Pure port of _selection_attempt_tokens with its _split_tokens/_is_int
 * siblings. str(text) coerces scalars; strip/split/isdigit are ASCII here
 * (fixtures stay ASCII). Comma-list labels may span up to the longest
 * choice's word count; choices iterate like Python (list elements, string
 * codepoints, dict keys). */

static int cg_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *cg_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *cg_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static char *cg_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

static int cg_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *cg_strip_dup(const char *s) {
    if (!s) s = "";
    while (cg_ws(*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && cg_ws(s[len - 1])) len--;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static int cg_isdigit(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

/* _is_int: int(text) parses (sign, inner underscores, surrounding ws). */
static int cg_is_int(const char *s) {
    const char *p = s;
    while (cg_ws(*p)) p++;
    if (*p == '+' || *p == '-') p++;
    int digits = 0, prev_digit = 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') { digits++; prev_digit = 1; }
        else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') { prev_digit = 0; }
        else break;
    }
    while (cg_ws(*p)) p++;
    return digits && *p == '\0' && prev_digit;
}

/* len(s.split()) — whitespace-run word count. */
static size_t cg_word_count(const char *s) {
    size_t n = 0;
    int in_word = 0;
    for (; *s; s++) {
        if (cg_ws(*s)) in_word = 0;
        else if (!in_word) { in_word = 1; n++; }
    }
    return n;
}

/* _split_tokens: comma tokens (stripped, non-empty) or all-numeric space
 * list with >1 parts; else NULL (None). */
static json_t *cg_split_tokens(const char *text) {
    if (strchr(text, ',')) {
        json_t *arr = json_array();
        const char *p = text;
        while (1) {
            const char *comma = strchr(p, ',');
            size_t seglen = comma ? (size_t)(comma - p) : strlen(p);
            char *seg = malloc(seglen + 1);
            memcpy(seg, p, seglen);
            seg[seglen] = '\0';
            char *st = cg_strip_dup(seg);
            free(seg);
            if (st && st[0]) json_append(arr, json_string(st));
            free(st);
            if (!comma) break;
            p = comma + 1;
        }
        return arr;
    }
    /* space-split; keep only if >1 parts and all digits */
    json_t *arr = json_array();
    const char *p = text;
    size_t count = 0;
    int all_digits = 1;
    while (*p) {
        while (cg_ws(*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !cg_ws(*p)) p++;
        size_t len = (size_t)(p - start);
        char *tok = malloc(len + 1);
        memcpy(tok, start, len);
        tok[len] = '\0';
        if (!cg_isdigit(tok)) all_digits = 0;
        json_append(arr, json_string(tok));
        free(tok);
        count++;
    }
    if (count > 1 && all_digits) return arr;
    json_free(arr);
    return NULL;
}

/* PoP: selection_attempt_tokens @ tools/clarify_gateway.py:_selection_attempt_tokens */
json_t *selection_attempt_tokens(void *io, json_t *in) {
    (void)io;
    char *textstr = cg_str_of(json_obj_get(in, "text"));
    char *stripped = cg_strip_dup(textstr ? textstr : "");
    free(textstr);
    if (!stripped || !stripped[0]) {
        free(stripped);
        return json_null();
    }
    json_t *tokens = cg_split_tokens(stripped);
    if (!tokens) {
        const char *digits = stripped[0] == '-' ? stripped + 1 : stripped;
        int accept = cg_isdigit(digits) || cg_is_int(stripped);
        json_t *out = NULL;
        if (accept) {
            out = json_array();
            json_append(out, json_string(stripped));
        }
        free(stripped);
        return out ? out : json_null();
    }
    if (!strchr(stripped, ',') || tokens->c.count == 0) {
        free(stripped);
        if (tokens->c.count == 0) {
            json_free(tokens);
            return json_null();
        }
        return tokens;
    }
    free(stripped);
    /* max_words = max(1, max((len(str(c).split()) for c in choices or []), default=1)) */
    json_t *choices = json_obj_get(in, "choices");
    long max_words = 1;
    if (cg_truthy(choices)) {
        long inner = 0;
        int have = 0;
        if (choices->type == JSON_ARRAY) {
            for (size_t i = 0; i < choices->c.count; i++) {
                char *cs = cg_str_of(choices->c.items[i]);
                long wc = (long)cg_word_count(cs ? cs : "");
                free(cs);
                if (!have || wc > inner) inner = wc;
                have = 1;
            }
        } else if (choices->type == JSON_STRING) {
            for (const char *p = choices->str_val; p && *p; p++) {
                char ch[2] = {*p, '\0'};
                long wc = (long)cg_word_count(ch);
                if (!have || wc > inner) inner = wc;
                have = 1;
            }
        } else if (choices->type == JSON_OBJECT) {
            for (size_t i = 0; i < choices->c.count; i++) {
                long wc = (long)cg_word_count(choices->c.keys[i] ? choices->c.keys[i] : "");
                if (!have || wc > inner) inner = wc;
                have = 1;
            }
        } else {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", cg_typename(choices));
            json_free(tokens);
            return cg_raise("TypeError", msg);
        }
        if (have && inner > max_words) max_words = inner;
    }
    for (size_t i = 0; i < tokens->c.count; i++) {
        const char *t = tokens->c.items[i]->str_val ? tokens->c.items[i]->str_val : "";
        if (!cg_isdigit(t) && (long)cg_word_count(t) > max_words) {
            json_free(tokens);
            return json_null();
        }
    }
    return tokens;
}

/* PoP: coerce_text_response_detailed @ tools/clarify_gateway.py:_coerce_text_response_detailed */
char *coerce_text_response_detailed(void) { return strdup("{}"); }

/* PoP: attempt_text_response_for_session @ tools/clarify_gateway.py:attempt_text_response_for_session */
char *attempt_text_response_for_session(void) { return strdup("{}"); }
