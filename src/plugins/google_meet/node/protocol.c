/*
 * protocol.c - C11 port of upstream plugins/google_meet/node/protocol.py
 * (gate w24_protocol_13d3cf).
 *
 * PoP: w24_protocol_13d3cf_nonempty_str @ plugins/google_meet/node/protocol.py:_nonempty_str
 * PoP: w24_protocol_13d3cf_make_request @ plugins/google_meet/node/protocol.py:make_request
 * PoP: w24_protocol_13d3cf_make_response @ plugins/google_meet/node/protocol.py:make_response
 * PoP: w24_protocol_13d3cf_make_error @ plugins/google_meet/node/protocol.py:make_error
 * PoP: w24_protocol_13d3cf_encode @ plugins/google_meet/node/protocol.py:encode
 * PoP: w24_protocol_13d3cf_decode @ plugins/google_meet/node/protocol.py:decode
 * PoP: w24_protocol_13d3cf_validate_request @ plugins/google_meet/node/protocol.py:validate_request
 * PoP: w24_protocol_13d3cf_constants @ plugins/google_meet/node/protocol.py:VALID_REQUEST_TYPES
 *
 * The wire protocol is values in, values out: the closed request-type set, the
 * ordered check chain whose first failure is the reason sent back to the client
 * (order is part of the contract -- a bad type outranks a bad token), the three
 * envelope constructors and the compact codec.  The bearer-token comparison is
 * a byte comparison of the UTF-8 encodings, as hmac.compare_digest does over
 * them.  uuid4() never runs here: its hex arrives through the uuid_hex seam.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_protocol_13d3cf.h"

/* VALID_REQUEST_TYPES */
static const char *const PR_TYPES[W24_PROTOCOL_13D3CF_TYPE_COUNT] = {
    "ping", "say", "start_bot", "status", "stop", "transcript",
};

/* --- small string builder ------------------------------------------------- */
typedef struct { char *b; size_t n, cap; int oom; } pr_buf;

static void pr_bput(pr_buf *s, const char *t, size_t len) {
    if (s->oom) return;
    if (s->n + len + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 64;
        while (cap < s->n + len + 1) cap *= 2;
        char *nb = (char *)realloc(s->b, cap);
        if (!nb) { s->oom = 1; return; }
        s->b = nb;
        s->cap = cap;
    }
    memcpy(s->b + s->n, t, len);
    s->n += len;
    s->b[s->n] = '\0';
}

static void pr_bputs(pr_buf *s, const char *t) { pr_bput(s, t, strlen(t)); }

static char *pr_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Shortest float text that round-trips (what Python prints). */
static void pr_num_text(double v, char *out, size_t cap) {
    char tmp[64];
    int p;
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(out, cap, "%.0f", v);
        return;
    }
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) { snprintf(out, cap, "%s", tmp); return; }
    }
    snprintf(out, cap, "%.17g", v);
}

static void pr_repr(pr_buf *s, const json_t *v);

static void pr_repr_str(pr_buf *s, const char *t) {
    pr_bput(s, "'", 1);
    for (const char *p = t; *p; p++) {
        if (*p == '\'') pr_bputs(s, "\\'");
        else if (*p == '\\') pr_bputs(s, "\\\\");
        else if (*p == '\n') pr_bputs(s, "\\n");
        else if (*p == '\t') pr_bputs(s, "\\t");
        else pr_bput(s, p, 1);
    }
    pr_bput(s, "'", 1);
}

static void pr_repr(pr_buf *s, const json_t *v) {
    size_t i, n;
    char num[64];
    if (!v) { pr_bputs(s, "None"); return; }
    switch (v->type) {
    case JSON_NULL: pr_bputs(s, "None"); return;
    case JSON_BOOL: pr_bputs(s, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: pr_num_text(v->num_val, num, sizeof(num)); pr_bputs(s, num); return;
    case JSON_STRING: pr_repr_str(s, v->str_val ? v->str_val : ""); return;
    case JSON_ARRAY:
        pr_bput(s, "[", 1);
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) pr_bputs(s, ", ");
            pr_repr(s, json_get(v, i));
        }
        pr_bput(s, "]", 1);
        return;
    case JSON_OBJECT:
        pr_bput(s, "{", 1);
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) pr_bputs(s, ", ");
            pr_repr_str(s, json_object_get_key_at(v, i));
            pr_bputs(s, ": ");
            pr_repr(s, json_object_get_at(v, i));
        }
        pr_bput(s, "}", 1);
        return;
    }
}

/* str(v): a string verbatim, everything else through repr(). Caller frees. */
static char *pr_pystr(const json_t *v) {
    pr_buf s = {NULL, 0, 0, 0};
    if (v && v->type == JSON_STRING) return pr_dup(v->str_val ? v->str_val : "");
    pr_repr(&s, v);
    if (s.oom) { free(s.b); return NULL; }
    if (!s.b) return pr_dup("");
    return s.b;
}

/* Python truthiness. */
static int pr_truthy(const json_t *v) {
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

static int pr_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

/* _nonempty_str */
static int pr_nonempty(const json_t *v) {
    return pr_is_str(v) && v->str_val && v->str_val[0] != '\0';
}

static json_t *pr_error(const char *text) {
    json_t *o = json_object();
    json_set(o, "error", json_string(text));
    return o;
}

json_t *w24_protocol_13d3cf_nonempty_str(const json_t *value) {
    return json_bool(pr_nonempty(value));
}

json_t *w24_protocol_13d3cf_make_request(const json_t *type, const json_t *token,
                                         const json_t *payload, const json_t *req_id,
                                         const json_t *uuid_hex) {
    json_t *out;
    size_t i;
    int known = 0;

    if (!pr_nonempty(type)) return pr_error("type must be a non-empty string");
    for (i = 0; i < W24_PROTOCOL_13D3CF_TYPE_COUNT; i++)
        if (!strcmp(type->str_val, PR_TYPES[i])) { known = 1; break; }
    if (!known) {
        pr_buf s = {NULL, 0, 0, 0};
        json_t *o;
        pr_bputs(&s, "unknown request type: ");
        pr_repr(&s, type);
        o = pr_error(s.oom ? "unknown request type" : (s.b ? s.b : ""));
        free(s.b);
        return o;
    }
    if (!pr_is_str(token)) return pr_error("token must be a string");
    if (!payload || payload->type != JSON_OBJECT) return pr_error("payload must be a dict");

    out = json_object();
    json_set(out, "type", json_copy(type));
    /* id = req_id or uuid.uuid4().hex -- a falsy req_id takes the seam value. */
    if (pr_truthy(req_id)) json_set(out, "id", json_copy(req_id));
    else json_set(out, "id", pr_is_str(uuid_hex) ? json_copy(uuid_hex) : json_null());
    json_set(out, "token", json_copy(token));
    json_set(out, "payload", json_copy(payload));
    return out;
}

json_t *w24_protocol_13d3cf_make_response(const json_t *req_id, const json_t *payload) {
    json_t *out;
    if (!payload || payload->type != JSON_OBJECT) return pr_error("payload must be a dict");
    out = json_object();
    json_set(out, "type", json_string("response"));
    json_set(out, "id", req_id ? json_copy(req_id) : json_null());
    json_set(out, "payload", json_copy(payload));
    return out;
}

json_t *w24_protocol_13d3cf_make_error(const json_t *req_id, const json_t *error) {
    char *text = pr_pystr(error);
    json_t *out = json_object();
    json_set(out, "type", json_string("error"));
    json_set(out, "id", req_id ? json_copy(req_id) : json_null());
    json_set(out, "error", json_string(text ? text : ""));
    free(text);
    return out;
}

json_t *w24_protocol_13d3cf_encode(const json_t *msg) {
    char *text = msg ? json_dumps(msg, 0) : NULL;
    json_t *out = json_string(text ? text : "null");
    free(text);
    return out;
}

json_t *w24_protocol_13d3cf_decode(const json_t *raw) {
    json_t *parsed, *out;
    const char *s;
    char reason[128];

    out = json_object();
    if (!pr_is_str(raw)) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "reason", json_string("malformed JSON"));
        return out;
    }
    s = raw->str_val ? raw->str_val : "";
    parsed = json_parse(s, NULL);
    if (!parsed) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "reason", json_string("malformed JSON"));
        return out;
    }
    if (parsed->type != JSON_OBJECT) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "reason", json_string("envelope must be a JSON object"));
        json_free(parsed);
        return out;
    }
    {
        static const char *const keys[2] = {"type", "id"};
        size_t i;
        for (i = 0; i < 2; i++) {
            if (!pr_is_str(json_obj_get(parsed, keys[i]))) {
                snprintf(reason, sizeof(reason), "envelope missing string '%s'", keys[i]);
                json_set(out, "ok", json_bool(0));
                json_set(out, "reason", json_string(reason));
                json_free(parsed);
                return out;
            }
        }
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "envelope", json_copy(parsed));
    json_free(parsed);
    return out;
}

json_t *w24_protocol_13d3cf_validate_request(const json_t *msg, const json_t *expected_token) {
    json_t *out = json_array();
    const json_t *t, *token, *payload;
    char *want;
    size_t i;
    int known = 0;

    if (!msg || msg->type != JSON_OBJECT) {
        json_append(out, json_bool(0));
        json_append(out, json_string("envelope must be a dict"));
        return out;
    }
    t = json_obj_get(msg, "type");
    token = json_obj_get(msg, "token");

    /* Ordered, lazily evaluated: first failing check wins. */
    if (!pr_nonempty(t)) {
        json_append(out, json_bool(0));
        json_append(out, json_string("missing or non-string 'type'"));
        return out;
    }
    for (i = 0; i < W24_PROTOCOL_13D3CF_TYPE_COUNT; i++)
        if (!strcmp(t->str_val, PR_TYPES[i])) { known = 1; break; }
    if (!known) {
        pr_buf s = {NULL, 0, 0, 0};
        pr_bputs(&s, "unknown request type: ");
        pr_repr(&s, t);
        json_append(out, json_bool(0));
        json_append(out, json_string(s.oom ? "unknown request type" : (s.b ? s.b : "")));
        free(s.b);
        return out;
    }
    if (!pr_nonempty(json_obj_get(msg, "id"))) {
        json_append(out, json_bool(0));
        json_append(out, json_string("missing or non-string 'id'"));
        return out;
    }
    if (!pr_nonempty(token)) {
        json_append(out, json_bool(0));
        json_append(out, json_string("missing token"));
        return out;
    }
    /* hmac.compare_digest over the UTF-8 encodings of both sides. */
    want = pr_is_str(expected_token) ? pr_dup(expected_token->str_val ? expected_token->str_val : "")
                                     : pr_pystr(expected_token);
    if (!want || strcmp(token->str_val ? token->str_val : "", want) != 0) {
        free(want);
        json_append(out, json_bool(0));
        json_append(out, json_string("token mismatch"));
        return out;
    }
    free(want);
    payload = json_obj_get(msg, "payload");
    if (!payload || payload->type != JSON_OBJECT) {
        json_append(out, json_bool(0));
        json_append(out, json_string("payload must be a dict"));
        return out;
    }
    json_append(out, json_bool(1));
    json_append(out, json_string(""));
    return out;
}

json_t *w24_protocol_13d3cf_constants(void) {
    json_t *o = json_object();
    json_t *types = json_array();
    size_t i;
    for (i = 0; i < W24_PROTOCOL_13D3CF_TYPE_COUNT; i++) json_append(types, json_string(PR_TYPES[i]));
    json_set(o, "valid_request_types", types);
    json_set(o, "response_type", json_string("response"));
    json_set(o, "error_type", json_string("error"));
    return o;
}

