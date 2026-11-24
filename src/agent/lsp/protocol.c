/*
 * protocol.c - C11 port of upstream agent/lsp/protocol.py
 * (gate w26_protocol_45f875).
 *
 * PoP: w26_protocol_45f875_encode @ agent/lsp/protocol.py:encode_message
 * PoP: w26_protocol_45f875_make @ agent/lsp/protocol.py:make_notification / make_request / make_response / make_error_response
 * PoP: w26_protocol_45f875_classify @ agent/lsp/protocol.py:classify_message
 * PoP: w26_protocol_45f875_decode @ agent/lsp/protocol.py:read_message / _read_headers
 * PoP: w26_protocol_45f875_constants @ agent/lsp/protocol.py:<module constants>
 *
 * The async streams are the seam: the caller hands the bytes a StreamReader
 * would have produced and this file decides where each frame ends, which is
 * exactly what the live framer decides.  Everything else (envelope builders,
 * classifier, byte budgets) is a pure value -> value function.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_protocol_45f875.h"

/* ---------------------------------------------------------------- helpers */

/* Python str.strip() over ASCII whitespace. */
static void px_trim(const char **s, size_t *n) {
    const char *p = *s;
    size_t len = *n;
    while (len && (p[0] == ' ' || p[0] == '\t' || p[0] == '\n' || p[0] == '\r' ||
                   p[0] == '\f' || p[0] == '\v')) { p++; len--; }
    while (len && (p[len - 1] == ' ' || p[len - 1] == '\t' || p[len - 1] == '\n' ||
                   p[len - 1] == '\r' || p[len - 1] == '\f' || p[len - 1] == '\v')) len--;
    *s = p;
    *n = len;
}

static char *px_dup_lower(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    size_t i;
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';
    return out;
}

static char *px_dup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Strict UTF-8 (Python bytes.decode("utf-8")). */
static int px_utf8_ok(const char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t need, k;
        unsigned char lo = 0x80, hi = 0xBF;
        if (c < 0x80) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) {
            need = 1;
        } else if (c >= 0xE0 && c <= 0xEF) {
            need = 2;
            if (c == 0xE0) lo = 0xA0;
            if (c == 0xED) hi = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            need = 3;
            if (c == 0xF0) lo = 0x90;
            if (c == 0xF4) hi = 0x8F;
        } else {
            return 0;
        }
        if (i + need >= n) return 0;
        if ((unsigned char)s[i + 1] < lo || (unsigned char)s[i + 1] > hi) return 0;
        for (k = 2; k <= need; k++) {
            if ((unsigned char)s[i + k] < 0x80 || (unsigned char)s[i + k] > 0xBF) return 0;
        }
        i += need + 1;
    }
    return 1;
}

/* Python int(str) over an already-stripped header value. */
static int px_parse_int(const char *s, long long *out) {
    size_t i = 0, n = strlen(s);
    int neg = 0;
    long long v = 0;
    if (!n) return 0;
    if (s[0] == '+' || s[0] == '-') { neg = s[0] == '-'; i = 1; }
    if (i >= n) return 0;
    for (; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        if (v > (9223372036854775807LL - (s[i] - '0')) / 10) return 0;
        v = v * 10 + (s[i] - '0');
    }
    *out = neg ? -v : v;
    return 1;
}

/* ------------------------------------------------------------ encode_message */

json_t *w26_protocol_45f875_encode(const json_t *req) {
    const json_t *obj = json_obj_get(req, "obj");
    json_t *body_node, *out;
    char *body, *frame;
    size_t need;

    body_node = obj ? json_copy(obj) : json_null();
    body = json_serialize(body_node);
    json_free(body_node);
    if (!body) return json_string("");
    need = strlen("Content-Length: \r\n\r\n") + 24 + strlen(body) + 1;
    frame = (char *)malloc(need);
    if (!frame) { free(body); return json_string(""); }
    snprintf(frame, need, "Content-Length: %zu\r\n\r\n%s", strlen(body), body);
    out = json_string(frame);
    free(frame);
    free(body);
    return out;
}

/* -------------------------------------------------------- envelope builders */

static void px_set_maybe(json_t *o, const char *key, const json_t *v) {
    /* Python's `**({} if params is None else {"params": params})`. */
    if (!v || v->type == JSON_NULL) return;
    json_set(o, key, json_copy(v));
}

json_t *w26_protocol_45f875_make(const json_t *req) {
    const char *kind = json_get_str(req, "kind", "");
    const json_t *idv = json_obj_get(req, "id");
    const json_t *method = json_obj_get(req, "method");
    const json_t *params = json_obj_get(req, "params");
    const json_t *result = json_obj_get(req, "result");
    const json_t *data = json_obj_get(req, "data");
    json_t *o = json_object();

    json_set(o, "jsonrpc", json_string("2.0"));
    if (!strcmp(kind, "notification")) {
        json_set(o, "method", method ? json_copy(method) : json_null());
        px_set_maybe(o, "params", params);
        return o;
    }
    if (!strcmp(kind, "request")) {
        json_set(o, "id", idv ? json_copy(idv) : json_null());
        json_set(o, "method", method ? json_copy(method) : json_null());
        px_set_maybe(o, "params", params);
        return o;
    }
    if (!strcmp(kind, "response")) {
        json_set(o, "id", idv ? json_copy(idv) : json_null());
        json_set(o, "result", result ? json_copy(result) : json_null());
        return o;
    }
    if (!strcmp(kind, "error")) {
        json_t *err = json_object();
        json_set(err, "code", json_number(json_get_num(req, "code", 0.0)));
        json_set(err, "message", json_string(json_get_str(req, "message", "")));
        px_set_maybe(err, "data", data);
        json_set(o, "id", idv ? json_copy(idv) : json_null());
        json_set(o, "error", err);
        return o;
    }
    return json_string("UNKNOWN_KIND");
}

/* ---------------------------------------------------------- classify_message */

json_t *w26_protocol_45f875_classify(const json_t *req) {
    const json_t *msg = json_obj_get(req, "msg");
    json_t *out = json_array();
    const char *kind = "invalid";
    const json_t *key = NULL;

    if (msg && msg->type == JSON_OBJECT) {
        const char *jrpc = json_get_str(msg, "jsonrpc", "");
        if (!strcmp(jrpc, "2.0")) {
            if (json_has(msg, "id")) {
                key = json_obj_get(msg, "id");
                if (json_has(msg, "method")) {
                    kind = "request";
                } else if (json_has(msg, "result") || json_has(msg, "error")) {
                    kind = "response";
                } else {
                    kind = "invalid";
                    key = NULL;
                }
            } else if (json_has(msg, "method")) {
                kind = "notification";
                key = json_obj_get(msg, "method");
            }
        }
    }
    json_append(out, json_string(kind));
    json_append(out, key ? json_copy(key) : json_null());
    return out;
}

/* ------------------------------------------------------------- read_message */

typedef struct {
    const char *s;
    size_t len;
    size_t pos;
} px_wire;

/* Returns NULL on success, an error kind on broken framing; *eof set on a clean
 * EOF before any header byte, which ends the message stream. */
static const char *px_read_headers(px_wire *w, json_t *hdrs, int *eof) {
    size_t header_bytes = 0;
    *eof = 0;
    for (;;) {
        size_t start = w->pos, nl, i, line_len;
        int found = 0;
        nl = start;
        while (nl + 1 < w->len) {
            if (w->s[nl] == '\r' && w->s[nl + 1] == '\n') { found = 1; break; }
            nl++;
        }
        if (!found) {
            /* IncompleteReadError: clean only when nothing at all had started. */
            if (start >= w->len && json_object_size(hdrs) == 0) { *eof = 1; return NULL; }
            return "eof";
        }
        line_len = nl - start;
        header_bytes += line_len + 2;
        if (header_bytes > W26_PROTOCOL_45F875_MAX_HEADER_BYTES) return "header_block";
        w->pos = nl + 2;
        if (line_len == 0) return NULL;  /* blank line ends the header block */
        for (i = 0; i < line_len; i++) {
            if ((unsigned char)w->s[start + i] >= 0x80) return "non_ascii_header";
        }
        {
            const char *line = w->s + start;
            size_t colon = line_len;
            const char *key = line, *value = line + line_len;
            size_t key_len, value_len = 0;
            char *k, *v;
            for (i = 0; i < line_len; i++) {
                if (line[i] == ':') { colon = i; break; }
            }
            key_len = colon;
            if (colon < line_len) { value = line + colon + 1; value_len = line_len - colon - 1; }
            if (key_len == 0) return "malformed_header";
            px_trim(&key, &key_len);
            px_trim(&value, &value_len);
            k = px_dup_lower(key, key_len);
            v = px_dup(value, value_len);
            if (!k || !v) { free(k); free(v); return "malformed_header"; }
            json_set(hdrs, k, json_string(v));
            free(k);
            free(v);
        }
    }
}

json_t *w26_protocol_45f875_decode(const json_t *req) {
    const json_t *wire_node = json_obj_get(req, "wire");
    json_t *out = json_object();
    json_t *msgs = json_array();
    const char *err = NULL;
    px_wire w;

    w.s = (wire_node && wire_node->type == JSON_STRING && wire_node->str_val)
              ? wire_node->str_val : "";
    w.len = strlen(w.s);
    w.pos = 0;

    while (!err) {
        json_t *hdrs = json_object();
        const json_t *cl;
        long long n;
        int eof = 0;
        err = px_read_headers(&w, hdrs, &eof);
        if (err || eof) { json_free(hdrs); break; }
        cl = json_obj_get(hdrs, "content-length");
        if (!cl || cl->type != JSON_STRING) {
            json_free(hdrs);
            err = "missing_content_length";
            break;
        }
        if (!px_parse_int(cl->str_val ? cl->str_val : "", &n)) {
            json_free(hdrs);
            err = "non_integer_length";
            break;
        }
        if (n < 0 || n > W26_PROTOCOL_45F875_MAX_BODY_BYTES) {
            json_free(hdrs);
            err = "unreasonable_length";
            break;
        }
        if ((size_t)n > w.len - w.pos) {
            json_free(hdrs);
            err = "truncated_body";
            break;
        }
        {
            char *body = px_dup(w.s + w.pos, (size_t)n);
            json_t *parsed;
            if (!body) { json_free(hdrs); err = "truncated_body"; break; }
            w.pos += (size_t)n;
            if (!px_utf8_ok(body, (size_t)n)) {
                free(body);
                json_free(hdrs);
                err = "non_utf8";
                break;
            }
            parsed = json_parse(body, NULL);
            free(body);
            json_free(hdrs);
            if (!parsed) { err = "invalid_json"; break; }
            json_append(msgs, parsed);
        }
    }

    json_set(out, "messages", msgs);
    json_set(out, "error", err ? json_string(err) : json_null());
    return out;
}

/* ---------------------------------------------------------------- constants */

json_t *w26_protocol_45f875_constants(void) {
    json_t *o = json_object();
    json_set(o, "error_content_modified",
             json_number((double)W26_PROTOCOL_45F875_ERROR_CONTENT_MODIFIED));
    json_set(o, "error_method_not_found",
             json_number((double)W26_PROTOCOL_45F875_ERROR_METHOD_NOT_FOUND));
    json_set(o, "max_header_bytes", json_number((double)W26_PROTOCOL_45F875_MAX_HEADER_BYTES));
    json_set(o, "max_body_bytes", json_number((double)W26_PROTOCOL_45F875_MAX_BODY_BYTES));
    return o;
}
