/*
 * conversation_compression_scaffolding.c — C port of upstream Hermes
 * agent/conversation_compression_scaffolding.py.
 * PoP: convscaf_message_text @ agent/conversation_compression_scaffolding.py:_message_text
 * PoP: convscaf_has_scaffolding_flag @ agent/conversation_compression_scaffolding.py:has_scaffolding_flag
 * PoP: convscaf_is_user_role_scaffolding @ agent/conversation_compression_scaffolding.py:is_user_role_scaffolding
 *
 * Faithful port: Python truthiness, str() coercion of text parts, "\n".join,
 * flag predicates, str.strip() and startswith(tuple) are all reproduced.
 * Fixture domain: text parts are strings/integers/bools/null (str() of a
 * non-integral float is not represented; json doubles would lose the distinction).
 */
#include "noc_convscaf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* Python truthiness of a JSON value. */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

/* Python str() for the scalar domain used by text parts. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dup_str("");
    switch (v->type) {
        case JSON_NULL:   return dup_str("None");
        case JSON_BOOL:   return dup_str(v->bool_val ? "True" : "False");
        case JSON_STRING: return dup_str(v->str_val ? v->str_val : "");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%g", v->num_val);
            return dup_str(buf);
        default:          return dup_str("");
    }
}

static void buf_append(char **buf, size_t *len, size_t *cap, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 64;
        while (nc < *len + n + 1) nc *= 2;
        char *nb = (char *)realloc(*buf, nc);
        if (!nb) return;
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
}

json_t *convscaf_message_text(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return json_string("");
    json_t *content = json_obj_get(message, "content");
    if (!content) return json_string("");
    if (content->type == JSON_STRING)
        return json_string(content->str_val ? content->str_val : "");
    if (content->type != JSON_ARRAY) return json_string("");

    char *buf = (char *)malloc(1);
    size_t len = 0, cap = 1;
    if (buf) buf[0] = '\0';
    size_t joined = 0;
    for (size_t i = 0; i < content->c.count; i++) {
        json_t *part = content->c.items[i];
        if (!part || part->type != JSON_OBJECT) continue; /* `if isinstance(part, dict)` */
        json_t *text = json_obj_get(part, "text");
        json_t *val = py_truthy(text) ? text : json_obj_get(part, "content");
        if (!py_truthy(val)) val = NULL;                  /* ... or "" */
        char *s = py_str(val);
        if (joined++) buf_append(&buf, &len, &cap, "\n");
        buf_append(&buf, &len, &cap, s);
        free(s);
    }
    json_t *out = json_string(buf ? buf : "");
    free(buf);
    return out;
}

static const char *const SYNTHETIC_FLAGS[] = {
    "_todo_snapshot_synthetic", "_empty_recovery_synthetic",
    "_verification_stop_synthetic", "_pre_verify_synthetic",
    "_dropped_toolcall_nudge",
};

int convscaf_has_scaffolding_flag(const json_t *message) {
    if (!message || message->type != JSON_OBJECT) return 0;
    for (size_t i = 0; i < sizeof(SYNTHETIC_FLAGS) / sizeof(SYNTHETIC_FLAGS[0]); i++)
        if (py_truthy(json_obj_get(message, SYNTHETIC_FLAGS[i]))) return 1;
    return 0;
}

static const char *const SYNTHETIC_PREFIXES[] = {
    "[System: Your previous response was truncated",
    "[System: The previous response was cut off",
    "[System: Your previous tool call",
    "[Your active task list was preserved across context compression]",
    "[IMPORTANT: Background process ",
};

int convscaf_is_user_role_scaffolding(const json_t *message) {
    if (convscaf_has_scaffolding_flag(message)) return 1;
    json_t *t = convscaf_message_text(message);
    const char *s = t ? (t->str_val ? t->str_val : "") : "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    int matched = 0;
    for (size_t i = 0; i < sizeof(SYNTHETIC_PREFIXES) / sizeof(SYNTHETIC_PREFIXES[0]); i++) {
        const char *p = SYNTHETIC_PREFIXES[i];
        size_t pn = strlen(p);
        if (pn <= b - a && strncmp(s + a, p, pn) == 0) { matched = 1; break; }
    }
    json_free(t);
    return matched;
}
