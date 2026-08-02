#define _POSIX_C_SOURCE 200809L

#define _POSIX_C_SOURCE 200809L
/*
 * lsp_protocol.c — port of agent/lsp/protocol.py.
 *
 */
#include "lsp_common.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define LSP_HEADER_CAP (8 * 1024)        /* 8 KiB header block cap */
#define LSP_BODY_CAP   (64 * 1024 * 1024) /* 64 MiB body cap */

/* ── encode_message ─────────────────────────────────────────────────── */
/* PoP: lsp_encode_message @ agent/lsp/protocol.py:encode_message */
char *lsp_encode_message(const char *json_body)
{
    /* The C API takes an already-serialized, compact JSON string (callers
     * build it). Wrap with the Content-Length header directly — no re-parse.
     * (Python encode_message re-serializes a dict; here the body is pre-built.) */
    const char *body = json_body ? json_body : "{}";
    size_t blen = strlen(body);
    size_t cap = 32 + blen + 1;
    char *out = malloc(cap);
    int hdr = snprintf(out, cap, "Content-Length: %zu\r\n\r\n", blen);
    memcpy(out + hdr, body, blen);
    out[hdr + blen] = '\0';
    return out;
}

/* Read exactly n bytes into buf (blocking, retries on short reads). */
static int read_exact(int fd, char *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return -2; /* EOF */
        got += (size_t)r;
    }
    return 0;
}

/* ── read_message ───────────────────────────────────────────────────── */
/* PoP: lsp_read_message @ agent/lsp/protocol.py:read_message */
char *lsp_read_message(int fd, lsp_protocol_error_t *proto_err)
{
    if (proto_err) { proto_err->message = NULL; }

    /* Read header lines until blank line (CRLF CRLF). */
    char header_block[8193];
    size_t hb = 0;
    bool have_content_len = false;
    long content_len = 0;

    for (;;) {
        char line[8193];
        size_t ll = 0;
        /* read until CRLF */
        for (;;) {
            char b;
            ssize_t r = read(fd, &b, 1);
            if (r < 0) { if (errno == EINTR) continue; goto proto_fail_eof; }
            if (r == 0) { /* EOF */
                if (hb == 0) return NULL; /* clean EOF between messages */
                goto proto_fail;
            }
            if (b == '\r') {
                /* peek next; must be \n */
                char n;
                ssize_t r2 = read(fd, &n, 1);
                if (r2 < 0) { if (errno == EINTR) continue; goto proto_fail_eof; }
                if (r2 == 0) goto proto_fail_eof;
                if (n == '\n') break; /* end of this header line */
                /* lone \r not followed by \n — treat \r as content, keep n */
                if (ll < sizeof(line) - 1) line[ll++] = b;
                if (ll < sizeof(line) - 1) line[ll++] = n;
                continue;
            }
            if (ll < sizeof(line) - 1) line[ll++] = b;
            else { /* line too long; keep consuming to avoid stall */ }
        }
        line[ll] = '\0';

        /* 8 KiB header cap */
        hb += ll + 2;
        if (hb > LSP_HEADER_CAP) goto proto_fail;

        if (ll == 0) break; /* blank line ends header block */

        /* parse "Key: Value" */
        char *colon = strchr(line, ':');
        if (!colon) goto proto_fail;
        *colon = '\0';
        char *key = line;
        char *val = colon + 1;
        while (*val == ' ') val++;
        /* strip trailing \r if any (already consumed) */
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\r' || val[vlen-1] == ' '))
            val[--vlen] = '\0';

        /* lowercase key compare */
        for (char *p = key; *p; p++) *p = (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
        if (strcmp(key, "content-length") == 0) {
            errno = 0;
            char *end = NULL;
            long n = strtol(val, &end, 10);
            if (end == val || *end != '\0' || n < 0 || n > LSP_BODY_CAP)
                goto proto_fail;
            content_len = n;
            have_content_len = true;
        }
    }

    if (!have_content_len) goto proto_fail;

    /* Read exact body. */
    char *body = malloc((size_t)content_len + 1);
    if (read_exact(fd, body, (size_t)content_len) != 0) {
        free(body);
        goto proto_fail_eof;
    }
    body[content_len] = '\0';
    return body;

proto_fail_eof:
    if (proto_err) proto_err->message = strdup("unexpected EOF in LSP framing");
    return NULL;
proto_fail:
    if (proto_err) proto_err->message = strdup("malformed LSP framing");
    return NULL;
}

/* ── envelope builders ──────────────────────────────────────────────── */
/* PoP: lsp_make_request @ agent/lsp/protocol.py:make_request */
char *lsp_make_request(int id, const char *method, const char *params_json)
{
    const char *p = params_json ? params_json : "null";
    size_t cap = 64 + strlen(method) + strlen(p) + 1;
    char *s = malloc(cap);
    if (params_json)
        snprintf(s, cap, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":%s}",
                 id, method, p);
    else
        snprintf(s, cap, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\"}", id, method);
    return s;
}

/* PoP: lsp_make_notification @ agent/lsp/protocol.py:make_notification */
char *lsp_make_notification(const char *method, const char *params_json)
{
    const char *p = params_json ? params_json : "null";
    size_t cap = 64 + strlen(method) + strlen(p) + 1;
    char *s = malloc(cap);
    if (params_json)
        snprintf(s, cap, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}", method, p);
    else
        snprintf(s, cap, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\"}", method);
    return s;
}

/* PoP: lsp_make_response @ agent/lsp/protocol.py:make_response */
char *lsp_make_response(int id, const char *result_json)
{
    const char *r = result_json ? result_json : "null";
    size_t cap = 48 + strlen(r) + 1;
    char *s = malloc(cap);
    snprintf(s, cap, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, r);
    return s;
}

/* PoP: lsp_make_error_response @ agent/lsp/protocol.py:make_error_response */
char *lsp_make_error_response(int id, int code, const char *message,
                              const char *data_json)
{
    size_t cap = 64 + strlen(message) + (data_json ? strlen(data_json) : 0) + 1;
    char *s = malloc(cap);
    if (data_json)
        snprintf(s, cap,
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\",\"data\":%s}}",
                 id, code, message, data_json);
    else
        snprintf(s, cap,
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
                 id, code, message);
    return s;
}

/* ── classify_message ───────────────────────────────────────────────── */
/* PoP: lsp_classify_message @ agent/lsp/protocol.py:classify_message */
lsp_msg_kind_t lsp_classify_message(const char *json, char **out_key, int *out_id)
{
    if (out_key) *out_key = NULL;
    if (out_id) *out_id = 0;
    if (!json) return LSP_MSG_INVALID;
    json_t *m = json_parse(json, NULL);
    if (!m) return LSP_MSG_INVALID;
    json_t *jr = json_obj_get(m, "jsonrpc");
    if (!jr || !json_is_string(jr) || strcmp(json_string_value(jr), "2.0") != 0) {
        json_free(m); return LSP_MSG_INVALID;
    }
    bool has_id = json_obj_get(m, "id") != NULL;
    bool has_method = json_obj_get(m, "method") != NULL;
    bool has_result = json_obj_get(m, "result") != NULL;
    bool has_error = json_obj_get(m, "error") != NULL;

    lsp_msg_kind_t kind = LSP_MSG_INVALID;
    if (has_id && has_method) {
        kind = LSP_MSG_REQUEST;
        if (out_key) *out_key = strdup(json_obj_get(m, "method") ? json_string_value(json_obj_get(m,"method")) : "");
    } else if (has_id && (has_result || has_error)) {
        kind = LSP_MSG_RESPONSE;
        json_t *idn = json_obj_get(m, "id");
        if (out_id && idn && json_is_number(idn)) *out_id = (int)json_number_value(idn);
    } else if (has_method && !has_id) {
        kind = LSP_MSG_NOTIFICATION;
        if (out_key) *out_key = strdup(json_obj_get(m, "method") ? json_string_value(json_obj_get(m,"method")) : "");
    }
    json_free(m);
    return kind;
}
