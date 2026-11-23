/*
 * session_reactions.c — C home for upstream Hermes gateway/session_reactions.py.
 *
 * PoP: w12_sessionrea_844dd0_parse @ gateway/session_reactions.py:_parse
 */
#include "noc_w12_sessionrea_844dd0.h"

#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FIELD_COUNT 6
static const char *const FIELDS[FIELD_COUNT] = {
    "session_id", "profile", "row_id", "newest_role", "emoji", "author"
};

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() on ASCII whitespace; result is malloc'd, caller frees. */
static char *str_strip(const char *s) {
    if (!s) return NULL;
    while (*s && is_ws(*s)) ++s;
    size_t n = strlen(s);
    while (n && is_ws(s[n - 1])) --n;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int key_allowed(const char *k) {
    for (int i = 0; i < FIELD_COUNT; ++i)
        if (!strcmp(k, FIELDS[i])) return 1;
    return 0;
}

static int py_falsy(const json_t *v) {
    if (!v) return 1;
    switch (v->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default:          return 1;
    }
}

int w12_sessionrea_844dd0_parse(const struct json_t *params, struct json_t **out) {
    if (out) *out = NULL;
    if (!params || params->type != JSON_OBJECT) return -1;

    /* set(params) - _FIELDS must be empty. */
    for (size_t i = 0; i < json_object_size(params); ++i) {
        const char *k = json_object_get_key_at(params, i);
        if (!k || !key_allowed(k)) return -1;
    }

    json_t *rid_n = json_obj_get(params, "row_id");
    json_t *nr_n  = json_obj_get(params, "newest_role");
    json_t *em_n  = json_obj_get(params, "emoji");
    json_t *au_n  = json_obj_get(params, "author");

    /* row_id: None or a REAL Python int (transport: decimal string / integral number). */
    int row_id_is_none = (!rid_n || rid_n->type == JSON_NULL);
    int64_t row_id_val = 0;
    if (!row_id_is_none) {
        if (rid_n->type == JSON_STRING && rid_n->str_val) {
            char *end = NULL;
            errno = 0;
            long long v = strtoll(rid_n->str_val, &end, 10);
            while (*end && is_ws(*end)) ++end;
            if (errno || end == rid_n->str_val || *end != '\0') return -1;
            row_id_val = (int64_t)v;
        } else if (rid_n->type == JSON_NUMBER) {
            double d = rid_n->num_val;
            if (d != (double)(int64_t)d) return -1; /* float, not int */
            row_id_val = (int64_t)d;
        } else {
            return -1; /* bool / array / object: type(x) is not int */
        }
    }

    /* When row_id is None, newest_role MUST be 'user' or 'assistant'.
     * When row_id is an int, Python does NOT validate newest_role at all. */
    if (row_id_is_none) {
        if (!nr_n || nr_n->type != JSON_STRING || !nr_n->str_val) return -1;
        if (strcmp(nr_n->str_val, "user") && strcmp(nr_n->str_val, "assistant")) return -1;
    }

    /* emoji: None or a non-blank string; stored value is emoji.strip(). */
    char *emoji_trimmed = NULL;
    if (em_n && em_n->type != JSON_NULL) {
        if (em_n->type != JSON_STRING || !em_n->str_val) return -1;
        emoji_trimmed = str_strip(em_n->str_val);
        if (!emoji_trimmed || !*emoji_trimmed) { free(emoji_trimmed); return -1; }
    }

    /* author = params.get('author') or 'user'; then must be in {'user','agent'}. */
    const char *author = "user";
    if (!py_falsy(au_n)) {
        if (au_n->type != JSON_STRING || !au_n->str_val) return -1;
        if (strcmp(au_n->str_val, "user") && strcmp(au_n->str_val, "agent")) return -1;
        author = au_n->str_val;
    }

    json_t *result = json_object();
    if (row_id_is_none)
        json_set(result, "row_id", json_null());
    else
        json_set(result, "row_id", json_number((double)row_id_val));
    json_set(result, "newest_role", nr_n ? json_copy(nr_n) : json_null());
    json_set(result, "emoji", emoji_trimmed ? json_string(emoji_trimmed) : json_null());
    json_set(result, "author", json_string(author));
    free(emoji_trimmed);

    if (out) *out = result;
    return 0;
}
