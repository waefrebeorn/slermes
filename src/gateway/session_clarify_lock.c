/*
 * session_clarify_lock.c — faithful C11 port of upstream
 * gateway/session_clarify_lock.py.
 *
 * The owner sends a local viewer's clarify batch as one clarify server request;
 * Ink/Desktop answer it one question at a time with clarify.lock. A viewer
 * attached to the request's session with session:respond may lock answers. This
 * module validates the ids, coerces a non-string answer through
 * json.dumps(ensure_ascii=False), and maps lock_answer's result to a status.
 * Runtime collaborators are injected as data through the seam parameters.
 *
 * PoP: clarifylock_lock          @ gateway/session_clarify_lock.py:clarify_lock
 * PoP: clarifylock_coerce_answer @ gateway/session_clarify_lock.py:clarify_lock
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_clarifylock.h"

/* ---- Python-compatible json.dumps(value, ensure_ascii=False) ------------ */
typedef struct { char *buf; size_t len, cap; } cl_sb;

static void cl_putc(cl_sb *s, char c) {
    if (s->len + 1 >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 128;
        char *n = (char *)realloc(s->buf, s->cap);
        if (!n) return;
        s->buf = n;
    }
    s->buf[s->len++] = c;
}
static void cl_puts(cl_sb *s, const char *p) { while (p && *p) cl_putc(s, *p++); }

static void cl_quote(cl_sb *s, const char *str) {
    cl_putc(s, '"');
    for (const unsigned char *p = (const unsigned char *)(str ? str : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"':  cl_puts(s, "\\\""); break;
            case '\\': cl_puts(s, "\\\\"); break;
            case '\b': cl_puts(s, "\\b"); break;
            case '\f': cl_puts(s, "\\f"); break;
            case '\n': cl_puts(s, "\\n"); break;
            case '\r': cl_puts(s, "\\r"); break;
            case '\t': cl_puts(s, "\\t"); break;
            default:
                if (c < 32) { char e[8]; snprintf(e, sizeof(e), "\\u%04x", c); cl_puts(s, e); }
                else cl_putc(s, (char)c); /* ensure_ascii=False: keep UTF-8 raw */
        }
    }
    cl_putc(s, '"');
}

static void cl_dump(cl_sb *s, const json_t *v) {
    if (!v) { cl_puts(s, "null"); return; }
    switch (v->type) {
        case JSON_NULL:   cl_puts(s, "null"); break;
        case JSON_BOOL:   cl_puts(s, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char t[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(t, sizeof(t), "%.0f", v->num_val);
            else
                snprintf(t, sizeof(t), "%.17g", v->num_val);
            cl_puts(s, t);
            break;
        }
        case JSON_STRING: cl_quote(s, v->str_val); break;
        case JSON_ARRAY:
            cl_putc(s, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) cl_puts(s, ", ");
                cl_dump(s, v->c.items[i]);
            }
            cl_putc(s, ']');
            break;
        case JSON_OBJECT:
            cl_putc(s, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) cl_puts(s, ", ");
                cl_quote(s, v->c.keys[i]);
                cl_puts(s, ": ");
                cl_dump(s, v->c.items[i]);
            }
            cl_putc(s, '}');
            break;
    }
}

static char *cl_py_dumps(const json_t *v) {
    cl_sb s = {NULL, 0, 0};
    cl_dump(&s, v);
    if (!s.buf) return (char *)calloc(1, 1);
    s.buf[s.len] = '\0';
    return s.buf;
}

static json_t *refusal(const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string("AuthorityRefusal"));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static int valid_id(const json_t *params, const char *key) {
    json_t *v = json_obj_get(params, key);
    return v && v->type == JSON_STRING && v->str_val && v->str_val[0];
}

json_t *clarifylock_lock(const json_t *params, const json_t *remaining, int lock_raises) {
    /* The dispatcher already refused unknown keys / mistyped fields; a missing
     * or empty id is ours to refuse. */
    if (!valid_id(params, "request_id") || !valid_id(params, "question_id"))
        return refusal("invalid_params");

    /* answer coercion (not observable through the injected pure lock_answer):
     * answer = params.get('answer', '')
     * if answer is not None and not isinstance(answer, str):
     *     answer = json.dumps(answer, ensure_ascii=False) */
    json_t *answer = json_obj_get(params, "answer");
    if (answer && answer->type != JSON_NULL && answer->type != JSON_STRING) {
        char *dumped = cl_py_dumps(answer);
        free(dumped);
    }

    /* try: remaining = lock_answer(...) except ValueError: invalid_params */
    if (lock_raises) return refusal("invalid_params");

    /* if remaining is None: return {'status': 'expired'} */
    if (!remaining || remaining->type == JSON_NULL) {
        json_t *o = json_object();
        json_set(o, "status", json_string("expired"));
        return o;
    }

    /* return {'status': 'ok', 'remaining': remaining} */
    json_t *o = json_object();
    json_set(o, "status", json_string("ok"));
    json_set(o, "remaining", json_copy(remaining));
    return o;
}

json_t *clarifylock_coerce_answer(const json_t *answer) {
    /* params.get('answer', '') */
    if (!answer) return json_string("");
    if (answer->type == JSON_NULL) return json_null();
    if (answer->type == JSON_STRING) return json_copy(answer);
    char *dumped = cl_py_dumps(answer);
    json_t *r = json_string(dumped);
    free(dumped);
    return r;
}
