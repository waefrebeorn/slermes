/*
 * rewind.c — C11 port of upstream Hermes
 * persistence/sqlite/conversation/_store/rewind.py (gate w26_rewind_057872).
 *
 * PoP: w26_rewind_057872_plan_ordinal @ persistence/sqlite/conversation/_store/rewind.py:plan_user_turn_rewind
 * PoP: w26_rewind_057872_retry_text @ session_rules/turns.py:retryable_user_text
 * PoP: w26_rewind_057872_flatten_text @ session_rules/turns.py:flatten_message_text
 * PoP: w26_rewind_057872_non_scaffolding @ session_rules/turns.py:_is_ephemeral_scaffolding
 * PoP: w26_rewind_057872_constants @ persistence/sqlite/conversation/_store/rewind.py:<module constants>
 *
 * The StoreHolder, its reads (get_active_message_ids, get_messages_as_conversation),
 * the rewind write transaction and the live transcript live in the caller.  What
 * is ported is what their values decide: which user turn an ordinal addresses,
 * how many turns that undoes, which warm rows are scaffolding, and the two text
 * projections /retry and prefill are built from.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_rewind_057872.h"

#define RW_HISTORY_CHANGED "session history changed before the rewind could be persisted"
#define RW_TARGET_GONE "target user message is no longer in session history"
#define RW_DB_PERSISTED_MARKER "_db_persisted"
#define RW_MESSAGE_UID "message_uid"

static const char *const RW_TEXT_KEYS[] = {"text", "content", "input_text", "output_text", "summary_text"};
#define RW_TEXT_KEY_COUNT (sizeof(RW_TEXT_KEYS) / sizeof(RW_TEXT_KEYS[0]))

static const char *const RW_NON_TEXT_PART_TYPES[] = {"image", "image_url", "input_image", "audio", "input_audio"};
#define RW_NON_TEXT_COUNT (sizeof(RW_NON_TEXT_PART_TYPES) / sizeof(RW_NON_TEXT_PART_TYPES[0]))

static const char *const RW_RETRY_TEXT_TYPES[] = {"text", "input_text", "output_text"};
#define RW_RETRY_TEXT_TYPE_COUNT (sizeof(RW_RETRY_TEXT_TYPES) / sizeof(RW_RETRY_TEXT_TYPES[0]))

#define RW_EPHEMERAL_COUNT 7
static const char *const RW_EPHEMERAL_FLAGS[RW_EPHEMERAL_COUNT] = {
    "_empty_recovery_synthetic", "_empty_terminal_sentinel", "_thinking_prefill",
    "_verification_stop_synthetic", "_pre_verify_synthetic", "_kanban_stop_synthetic",
    "_dropped_toolcall_nudge",
};

static const char *const RW_PLAN_FIELDS[] = {
    "target_row_id", "expected_active_ids", "expected_target_content", "scaffold", "prefix",
    "durable_prefix", "live_view", "live_text", "durable_user", "user_ordinal",
};
static const char *const RW_OUTCOME_FIELDS[] = {
    "prefix", "live_view", "live_text", "rewound_count", "turns_undone",
};

/* ---- growable string ---- */

typedef struct {
    char *p;
    size_t n, cap;
} rw_buf;

static void rw_buf_add(rw_buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < b->n + n + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void rw_buf_adds(rw_buf *b, const char *s) { rw_buf_add(b, s, strlen(s)); }

static char *rw_buf_take(rw_buf *b) {
    char *out = b->p ? b->p : (char *)calloc(1, 1);
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

/* The fail-closed answer shape both text projections report. */
static json_t *rw_error(const char *msg) {
    json_t *res = json_object();
    json_set(res, "ok", json_bool(false));
    json_set(res, "error", json_string(msg));
    return res;
}

/* ---- Python truthiness / str() ---- */

static int rw_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Shortest round-tripping form of a float, as Python's repr() prints it.
 * libjson keeps every number as a double, so an integral value is printed as
 * the int Python would have parsed from the same JSON token. */
static void rw_float_repr(double v, char *buf, size_t cap) {
    int prec;
    if (v == (double)(long long)v && v > -1e16 && v < 1e16) {
        snprintf(buf, cap, "%.0f", v);
        return;
    }
    for (prec = 1; prec <= 17; prec++) {
        double back;
        snprintf(buf, cap, "%.*g", prec, v);
        back = strtod(buf, NULL);
        if (back == v) return;
    }
}

static void rw_repr(const json_t *v, rw_buf *out);

static void rw_repr_str(const char *s, rw_buf *out) {
    char quote = '\'';
    size_t i;
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    rw_buf_add(out, &quote, 1);
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\') rw_buf_adds(out, "\\\\");
        else if (c == '\n') rw_buf_adds(out, "\\n");
        else if (c == '\r') rw_buf_adds(out, "\\r");
        else if (c == '\t') rw_buf_adds(out, "\\t");
        else if ((char)c == quote) { rw_buf_add(out, "\\", 1); rw_buf_add(out, (const char *)&quote, 1); }
        else rw_buf_add(out, (const char *)&s[i], 1);
    }
    rw_buf_add(out, &quote, 1);
}

static void rw_repr(const json_t *v, rw_buf *out) {
    size_t i;
    if (!v || v->type == JSON_NULL) { rw_buf_adds(out, "None"); return; }
    if (v->type == JSON_BOOL) { rw_buf_adds(out, v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        char tmp[64];
        rw_float_repr(v->num_val, tmp, sizeof(tmp));
        rw_buf_adds(out, tmp);
        return;
    }
    if (v->type == JSON_STRING) { rw_repr_str(v->str_val, out); return; }
    if (v->type == JSON_ARRAY) {
        rw_buf_adds(out, "[");
        for (i = 0; i < json_len(v); i++) {
            if (i) rw_buf_adds(out, ", ");
            rw_repr(json_get(v, i), out);
        }
        rw_buf_adds(out, "]");
        return;
    }
    rw_buf_adds(out, "{");
    for (i = 0; i < json_object_size(v); i++) {
        const char *k = json_object_get_key_at(v, i);
        if (i) rw_buf_adds(out, ", ");
        rw_repr_str(k, out);
        rw_buf_adds(out, ": ");
        rw_repr(json_object_get_at(v, i), out);
    }
    rw_buf_adds(out, "}");
}

/* str(x or "").strip().lower() for a scalar shape flag. */
static int rw_str_in(const json_t *v, const char *const *set, size_t count) {
    char buf[256];
    size_t i, n = 0, start = 0, end;
    if (!v || v->type == JSON_NULL) buf[0] = '\0';
    else if (v->type == JSON_BOOL) snprintf(buf, sizeof(buf), "%s", v->bool_val ? "true" : "false");
    else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e16 && d < 1e16) snprintf(buf, sizeof(buf), "%.0f", d);
        else snprintf(buf, sizeof(buf), "%.17g", d);
    } else if (v->type == JSON_STRING)
        snprintf(buf, sizeof(buf), "%s", v->str_val ? v->str_val : "");
    else
        snprintf(buf, sizeof(buf), "%s", v->type == JSON_ARRAY ? "[]" : "{}");
    end = strlen(buf);
    while (start < end && isspace((unsigned char)buf[start])) start++;
    while (end > start && isspace((unsigned char)buf[end - 1])) end--;
    if (start > 0) memmove(buf, buf + start, end - start);
    buf[end - start] = '\0';
    for (n = 0; buf[n]; n++) buf[n] = (char)tolower((unsigned char)buf[n]);
    for (i = 0; i < count; i++)
        if (!strcmp(buf, set[i])) return 1;
    return 0;
}

/* ---- _field / _text_from_part ---- */

static const json_t *rw_field(const json_t *v, const char *key) {
    if (v && v->type == JSON_OBJECT) return json_obj_get(v, key);
    return NULL;
}

/* _text_from_part(part): "" for None / non-text parts / no text key. */
static char *rw_text_from_part(const json_t *part) {
    size_t i;
    if (!part || part->type == JSON_NULL) return NULL;
    if (part->type == JSON_STRING) {
        size_t n = strlen(part->str_val ? part->str_val : "") + 1;
        char *s = (char *)malloc(n);
        if (s) memcpy(s, part->str_val ? part->str_val : "", n);
        return s;
    }
    if (rw_str_in(rw_field(part, "type"), RW_NON_TEXT_PART_TYPES, RW_NON_TEXT_COUNT)) return NULL;
    for (i = 0; i < RW_TEXT_KEY_COUNT; i++) {
        const json_t *t = rw_field(part, RW_TEXT_KEYS[i]);
        if (t && t->type == JSON_STRING) {
            size_t n = strlen(t->str_val ? t->str_val : "") + 1;
            char *s = (char *)malloc(n);
            if (s) memcpy(s, t->str_val ? t->str_val : "", n);
            return s;
        }
    }
    return NULL;
}

json_t *w26_rewind_057872_flatten_text(const json_t *content) {
    rw_buf out = {NULL, 0, 0};
    if (!content || content->type == JSON_NULL) return json_string("");
    if (content->type == JSON_STRING) return json_string(content->str_val ? content->str_val : "");
    if (content->type == JSON_ARRAY) {
        size_t i, first = 1;
        for (i = 0; i < json_len(content); i++) {
            char *chunk = rw_text_from_part(json_get(content, i));
            if (chunk && chunk[0]) {
                if (!first) rw_buf_adds(&out, "\n");
                rw_buf_adds(&out, chunk);
                first = 0;
            }
            free(chunk);
        }
    } else {
        char *text = rw_text_from_part(content);
        if (text && text[0]) {
            rw_buf_adds(&out, text);
            free(text);
        } else {
            free(text);
            rw_repr(content, &out);
        }
    }
    {
        char *s = rw_buf_take(&out);
        json_t *res = json_string(s ? s : "");
        free(s);
        return res;
    }
}

json_t *w26_rewind_057872_retry_text(const json_t *content) {
    rw_buf out = {NULL, 0, 0};
    char *text;
    json_t *res;
    size_t i;

    if (!content || (content->type != JSON_STRING && content->type != JSON_ARRAY))
        return rw_error("retry does not support non-text content");
    if (content->type == JSON_STRING) {
        rw_buf_adds(&out, content->str_val ? content->str_val : "");
    } else {
        for (i = 0; i < json_len(content); i++) {
            const json_t *part = json_get(content, i);
            const json_t *type, *ptext;
            size_t k;
            if (part && part->type == JSON_STRING) {
                rw_buf_adds(&out, part->str_val ? part->str_val : "");
                continue;
            }
            if (!part || part->type != JSON_OBJECT) {
                free(out.p);
                return rw_error("retry does not support non-text content");
            }
            type = json_obj_get(part, "type");
            if (!rw_str_in(type, RW_RETRY_TEXT_TYPES, RW_RETRY_TEXT_TYPE_COUNT)) {
                free(out.p);
                return rw_error("retry does not support media or unknown content parts");
            }
            for (k = 0; k < json_object_size(part); k++) {
                const char *key = json_object_get_key_at(part, k);
                if (!key || (!strcmp(key, "type") || !strcmp(key, "text"))) continue;
                free(out.p);
                return rw_error("retry cannot losslessly flatten annotated text parts");
            }
            ptext = json_obj_get(part, "text");
            if (!ptext || ptext->type != JSON_STRING) {
                free(out.p);
                return rw_error("retry text parts must contain text");
            }
            rw_buf_adds(&out, ptext->str_val ? ptext->str_val : "");
        }
    }
    text = rw_buf_take(&out);
    {
        const char *p = text;
        int blank = 1;
        while (p && *p) {
            if (!isspace((unsigned char)*p)) { blank = 0; break; }
            p++;
        }
        if (blank) {
            free(text);
            return rw_error("retry found no text to send");
        }
    }
    res = json_object();
    json_set(res, "ok", json_bool(true));
    json_set(res, "text", json_string(text ? text : ""));
    free(text);
    return res;
}

json_t *w26_rewind_057872_plan_ordinal(const json_t *count, const json_t *ordinal) {
    long long n, o;
    json_t *res;

    if (!count || count->type != JSON_NUMBER) return rw_error(RW_HISTORY_CHANGED);
    if (!ordinal || ordinal->type != JSON_NUMBER) return rw_error(RW_TARGET_GONE);
    n = (long long)count->num_val;
    o = (long long)ordinal->num_val;
    if (o < 0) {
        o = n + o;
        if (o < 0) o = 0;
    }
    if (o >= n) return rw_error(RW_TARGET_GONE);
    res = json_object();
    json_set(res, "ok", json_bool(true));
    json_set(res, "ordinal", json_number((double)o));
    json_set(res, "turns_undone", json_number((double)(n - o)));
    return res;
}

json_t *w26_rewind_057872_non_scaffolding(const json_t *messages) {
    json_t *out = json_array();
    size_t i, k;
    if (!messages || messages->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(messages); i++) {
        const json_t *msg = json_get(messages, i);
        int scaffold = 0;
        if (msg && msg->type == JSON_OBJECT) {
            for (k = 0; k < RW_EPHEMERAL_COUNT; k++)
                if (rw_truthy(json_obj_get(msg, RW_EPHEMERAL_FLAGS[k]))) { scaffold = 1; break; }
        }
        if (!scaffold) json_append(out, msg ? json_copy(msg) : json_null());
    }
    return out;
}

json_t *w26_rewind_057872_constants(void) {
    json_t *o = json_object();
    json_t *plan = json_array();
    json_t *outcome = json_array();
    json_t *flags = json_array();
    size_t i;

    for (i = 0; i < sizeof(RW_PLAN_FIELDS) / sizeof(RW_PLAN_FIELDS[0]); i++)
        json_append(plan, json_string(RW_PLAN_FIELDS[i]));
    for (i = 0; i < sizeof(RW_OUTCOME_FIELDS) / sizeof(RW_OUTCOME_FIELDS[0]); i++)
        json_append(outcome, json_string(RW_OUTCOME_FIELDS[i]));
    for (i = 0; i < RW_EPHEMERAL_COUNT; i++) json_append(flags, json_string(RW_EPHEMERAL_FLAGS[i]));

    json_set(o, "history_changed", json_string(RW_HISTORY_CHANGED));
    json_set(o, "db_persisted_marker", json_string(RW_DB_PERSISTED_MARKER));
    json_set(o, "message_uid", json_string(RW_MESSAGE_UID));
    json_set(o, "plan_fields", plan);
    json_set(o, "outcome_fields", outcome);
    json_set(o, "ephemeral_flags", flags);
    return o;
}
