/*
 * media_download.c -- real C11 port of the size-capped inbound ``mxc://`` body
 * reader of upstream Hermes plugins/platforms/matrix/media_download.py.
 *
 * PoP: w21_mediadownl_8ef7a9_read_body_capped @ plugins/platforms/matrix/media_download.py:read_body_capped
 * PoP: w21_mediadownl_8ef7a9_chunk_size @ plugins/platforms/matrix/media_download.py:_CHUNK_SIZE
 *
 * `download_mxc_capped` (Client.versions, api.get_download_url, api.session.get,
 * log_download_request*) is bound to a live mautrix client and HTTP session, so
 * it stays upstream.  The size discipline itself -- declared Content-Length
 * first, then the running total as chunks stream in -- is ported verbatim.
 */
#include "hermes_json.h"
#include "noc_w21_mediadownl_8ef7a9.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* int() of a header value; every failure mode Python swallows becomes 0. */
static int md8a_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

static int md8a_py_int(const json_t *v, long long *out) {
    *out = 0;
    if (!v) return 1;
    switch (v->type) {
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long long parsed;
        size_t i, len;
        /* int(s) tolerates surrounding whitespace and single underscores
         * between digits ("1_0" == 10) but nothing else. */
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        len = strlen(s);
        while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' ||
                           s[len - 1] == '\r' || s[len - 1] == '\f' || s[len - 1] == '\v')) {
            len--;
        }
        if (len == 0) return 1;
        for (i = 0; i < len; i++) {
            char c = s[i];
            if (c >= '0' && c <= '9') continue;
            if (c == '_' && i > 0 && i + 1 < len &&
                s[i - 1] >= '0' && s[i - 1] <= '9' &&
                s[i + 1] >= '0' && s[i + 1] <= '9') continue;
            if ((c == '+' || c == '-') && i == 0 && len > 1) continue;
            return 1;                              /* ValueError -> declared = 0 */
        }
        {
            char tmp[64];
            size_t j = 0;
            for (i = 0; i < len && j < sizeof(tmp) - 1; i++) {
                if (s[i] != '_') tmp[j++] = s[i];
            }
            tmp[j] = '\0';
            parsed = strtoll(tmp, &end, 10);
            if (end && *end == '\0') {
                *out = parsed;
            } else {
                /* out of long long range: clamp, the comparison only needs a bound */
                *out = (tmp[0] == '-') ? LLONG_MIN : LLONG_MAX;
            }
        }
        return 1;
    }
    case JSON_NUMBER:
        *out = (long long)v->num_val;              /* int(float) truncates toward zero */
        return 1;
    case JSON_BOOL:
        *out = v->bool_val ? 1 : 0;
        return 1;
    default:
        return 1;                                  /* TypeError -> declared = 0 */
    }
}

static const char *md8a_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) return "int";
        return "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

static json_t *md8a_error(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

static json_t *md8a_too_large(const char *message) {
    return md8a_error("MatrixMediaTooLarge", message);
}

static json_t *md8a_ok(const char *buf, size_t len) {
    static const char HEX[] = "0123456789abcdef";
    json_t *out = json_object();
    char *hex = (char *)malloc(len * 2 + 1);
    char num[32];
    size_t i;
    if (!hex) return md8a_error("MemoryError", "");
    for (i = 0; i < len; i++) {
        unsigned char b = (unsigned char)buf[i];
        hex[i * 2] = HEX[b >> 4];
        hex[i * 2 + 1] = HEX[b & 0x0f];
    }
    hex[len * 2] = '\0';
    snprintf(num, sizeof(num), "%lld", (long long)len);
    json_set(out, "status", json_string("ok"));
    json_set(out, "size", json_string(num));
    json_set(out, "body_hex", json_string(hex));
    free(hex);
    return out;
}

int w21_mediadownl_8ef7a9_chunk_size(void) {
    return 65536;
}

json_t *w21_mediadownl_8ef7a9_read_body_capped(const json_t *headers,
                                               const json_t *chunks,
                                               const json_t *limit) {
    json_t *declared_node;
    long long declared = 0, cap;
    size_t cap_len = 256, used = 0;
    char *buf;

    if (!headers || headers->type != JSON_OBJECT) {
        char msg[96];
        snprintf(msg, sizeof(msg), "'%s' object has no attribute 'get'",
                 md8a_py_type_name(headers));
        return md8a_error("AttributeError", msg);
    }
    if (!limit || (limit->type != JSON_NUMBER && limit->type != JSON_BOOL)) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "'>' not supported between instances of 'int' and '%s'",
                 md8a_py_type_name(limit));
        return md8a_error("TypeError", msg);
    }
    cap = (long long)limit->num_val;

    declared_node = json_obj_get(headers, "Content-Length");
    if (!md8a_truthy(declared_node)) {
        /* `headers.get("Content-Length") or headers.get("content-length") or 0` */
        declared_node = json_obj_get(headers, "content-length");
    }
    md8a_py_int(declared_node, &declared);
    if (declared > cap) {
        char msg[160];
        snprintf(msg, sizeof(msg), "media exceeds Matrix limit (%lld > %lld bytes)",
                 declared, cap);
        return md8a_too_large(msg);
    }

    if (!chunks || chunks->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "'async for' requires an object with __aiter__ method, got %s",
                 md8a_py_type_name(chunks));
        return md8a_error("TypeError", msg);
    }

    buf = (char *)malloc(cap_len);
    if (!buf) return md8a_error("MemoryError", "");

    {
        size_t i;
        for (i = 0; i < chunks->c.count; i++) {
            json_t *chunk = chunks->c.items[i];
            size_t clen = 0;
            long long total;

            if (chunk && chunk->type == JSON_STRING) {
                clen = strlen(chunk->str_val ? chunk->str_val : "");
            } else if (chunk && chunk->type == JSON_ARRAY) {
                clen = chunk->c.count;             /* len(list); bytes() range-checked below */
                {
                    size_t k;
                    for (k = 0; k < chunk->c.count; k++) {
                        json_t *e = chunk->c.items[k];
                        if (!e || e->type != JSON_NUMBER) continue;
                        if (e->num_val < 0.0 || e->num_val > 255.0) {
                            free(buf);
                            return md8a_error("ValueError", "bytes must be in range(0, 256)");
                        }
                    }
                }
            } else {
                char msg[96];
                free(buf);
                snprintf(msg, sizeof(msg), "object of type '%s' has no len()",
                         md8a_py_type_name(chunk));
                return md8a_error("TypeError", msg);
            }

            total = (long long)used + (long long)clen;
            if (total > cap) {
                char msg[128];
                free(buf);
                snprintf(msg, sizeof(msg), "media exceeds Matrix limit (> %lld bytes)", cap);
                return md8a_too_large(msg);
            }
            if (used + clen + 1 > cap_len) {
                size_t want = cap_len;
                char *grown;
                while (want < used + clen + 1) want *= 2;
                grown = (char *)realloc(buf, want);
                if (!grown) {
                    free(buf);
                    return md8a_error("MemoryError", "");
                }
                buf = grown;
                cap_len = want;
            }
            if (chunk->type == JSON_STRING) {
                if (clen) memcpy(buf + used, chunk->str_val, clen);
            } else {
                size_t k;
                for (k = 0; k < chunk->c.count; k++) {
                    json_t *e = chunk->c.items[k];
                    buf[used + k] = (char)(unsigned char)(e && e->type == JSON_NUMBER
                                                         ? (long long)e->num_val : 0);
                }
            }
            used += clen;
        }
    }

    {
        json_t *out = md8a_ok(buf, used);
        free(buf);
        return out;
    }
}
