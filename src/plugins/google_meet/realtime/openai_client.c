/*
 * openai_client.c -- real C11 port of upstream Hermes
 *   plugins/google_meet/realtime/openai_client.py (pure half).
 *
 * ``RealtimeSession.connect``/``speak``/``cancel_response``/``_recv_frame`` are live
 * WebSocket wrappers and are not ported.  The pure helpers the speaker queue is made
 * of -- audio-frame decoding and the JSONL queue's read/rewrite/append formatting --
 * are ported exactly, including CPython's ``json.dumps`` wire form (``", "`` /
 * ``": "`` separators, ``ensure_ascii`` escaping).
 *
 * PoP: w19_openaiclie_7f783c_decode_audio @ plugins/google_meet/realtime/openai_client.py:_decode_audio
 * PoP: w19_openaiclie_7f783c_read_queue @ plugins/google_meet/realtime/openai_client.py:RealtimeSpeaker._read_queue
 * PoP: w19_openaiclie_7f783c_rewrite_queue @ plugins/google_meet/realtime/openai_client.py:RealtimeSpeaker._rewrite_queue
 * PoP: w19_openaiclie_7f783c_append_processed @ plugins/google_meet/realtime/openai_client.py:RealtimeSpeaker._append_processed
 */
#include "hermes_json.h"
#include "noc_w19_openaiclie_7f783c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Module-level literal mirrored from the Python source. */
static const char *const REALTIME_URL = "wss://api.openai.com/v1/realtime";

/* PoP: w19_openaiclie_7f783c_realtime_url @ plugins/google_meet/realtime/openai_client.py:REALTIME_URL */
const char *w19_openaiclie_7f783c_realtime_url(void) { return REALTIME_URL; }

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* -- base64 (binascii.a2b_base64, the lenient half b64decode uses) ------------ */

static int b64val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Decoded bytes (malloc'd, NUL-terminated) or NULL when the string cannot decode. */
static char *b64_decode(const char *s, size_t *out_len) {
    size_t cap = strlen(s) / 4 * 3 + 4, o = 0;
    unsigned char *out = (unsigned char *)malloc(cap ? cap : 1);
    int group[4], n = 0, padded = 0, err = 0;
    const char *p;
    if (!out) return NULL;
    for (p = s ? s : ""; *p; p++) {
        int v;
        if (*p == '=') {
            if (padded || n == 0) continue;
            if (n == 1) { err = 1; break; }
            out[o++] = (unsigned char)((group[0] << 2) | (group[1] >> 4));
            if (n == 3) out[o++] = (unsigned char)(((group[1] & 0xF) << 4) | (group[2] >> 2));
            n = 0;
            padded = 1;
            continue;
        }
        v = b64val((unsigned char)*p);
        if (v < 0) continue;              /* discarded, exactly like a2b_base64 */
        group[n++] = v;
        if (n == 4) {
            out[o++] = (unsigned char)((group[0] << 2) | (group[1] >> 4));
            out[o++] = (unsigned char)(((group[1] & 0xF) << 4) | (group[2] >> 2));
            out[o++] = (unsigned char)(((group[2] & 0x3) << 6) | group[3]);
            n = 0;
        }
    }
    if (!err && n != 0) err = 1;
    if (err) { free(out); return NULL; }
    out[o] = 0;
    *out_len = o;
    return (char *)out;
}

json_t *w19_openaiclie_7f783c_decode_audio(const char *b64) {
    json_t *out = json_object();
    static const char HEXD[] = "0123456789abcdef";
    size_t n = 0, i;
    char *bytes = (b64 && *b64) ? b64_decode(b64, &n) : NULL;
    char *hex = (char *)malloc(n * 2 + 1);
    if (n == 0) {
        json_set(out, "hex", json_string(""));
        free(bytes);
        free(hex);
        return out;
    }
    if (!hex) { free(bytes); json_set(out, "hex", json_string("")); return out; }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)bytes[i];
        hex[i * 2] = HEXD[c >> 4];
        hex[i * 2 + 1] = HEXD[c & 0xF];
    }
    hex[n * 2] = '\0';
    json_set(out, "hex", json_string(hex));
    free(hex);
    free(bytes);
    return out;
}

/* -- CPython json.dumps wire form -------------------------------------------- */

typedef struct {
    char *buf;
    size_t len, cap;
    int oom;
} sb_t;

static void sb_reserve(sb_t *b, size_t extra) {
    size_t need, nc;
    char *nb;
    if (b->oom) return;
    need = b->len + extra + 1;
    if (need <= b->cap) return;
    nc = b->cap ? b->cap * 2 : 64;
    while (nc < need) nc *= 2;
    nb = (char *)realloc(b->buf, nc);
    if (!nb) { b->oom = 1; return; }
    b->buf = nb;
    b->cap = nc;
}

static void sb_mem(sb_t *b, const char *s, size_t n) {
    sb_reserve(b, n);
    if (b->oom) return;
    if (n) memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void sb_ch(sb_t *b, char c) { sb_mem(b, &c, 1); }
static void sb_cstr(sb_t *b, const char *s) { sb_mem(b, s, strlen(s)); }

static size_t u8_next(const char *s, unsigned *cp) {
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x80) { *cp = u[0]; return 1; }
    if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x1F) << 6) | (unsigned)(u[1] & 0x3F);
        return 2;
    }
    if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x0F) << 12) | ((unsigned)(u[1] & 0x3F) << 6) | (unsigned)(u[2] & 0x3F);
        return 3;
    }
    if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x07) << 18) | ((unsigned)(u[1] & 0x3F) << 12) |
              ((unsigned)(u[2] & 0x3F) << 6) | (unsigned)(u[3] & 0x3F);
        return 4;
    }
    *cp = u[0];
    return 1;
}

/* json.dumps(str): ensure_ascii=True */
static void sb_pystr(sb_t *b, const char *s) {
    unsigned cp;
    char esc[16];
    sb_ch(b, '"');
    if (s) {
        while (*s) {
            size_t step = u8_next(s, &cp);
            switch (cp) {
                case '"':  sb_cstr(b, "\\\""); break;
                case '\\': sb_cstr(b, "\\\\"); break;
                case '\n': sb_cstr(b, "\\n"); break;
                case '\r': sb_cstr(b, "\\r"); break;
                case '\t': sb_cstr(b, "\\t"); break;
                case '\b': sb_cstr(b, "\\b"); break;
                case '\f': sb_cstr(b, "\\f"); break;
                default:
                    if (cp < 0x20) {
                        snprintf(esc, sizeof esc, "\\u%04x", cp);
                        sb_cstr(b, esc);
                    } else if (cp < 0x80) {
                        sb_ch(b, (char)cp);
                    } else if (cp <= 0xFFFF) {
                        snprintf(esc, sizeof esc, "\\u%04x", cp);
                        sb_cstr(b, esc);
                    } else {
                        unsigned v = cp - 0x10000;
                        snprintf(esc, sizeof esc, "\\u%04x\\u%04x",
                                 0xD800 | (v >> 10), 0xDC00 | (v & 0x3FF));
                        sb_cstr(b, esc);
                    }
                    break;
            }
            s += step;
        }
    }
    sb_ch(b, '"');
}

static void sb_json(sb_t *b, const json_t *v);

static void sb_number(sb_t *b, double d) {
    char tmp[64];
    if (d == (double)(long long)d && d < 1e15 && d > -1e15)
        snprintf(tmp, sizeof tmp, "%lld", (long long)d);
    else
        snprintf(tmp, sizeof tmp, "%.17g", d);
    sb_cstr(b, tmp);
}

static void sb_json(sb_t *b, const json_t *v) {
    size_t i;
    if (!v || v->type == JSON_NULL) { sb_cstr(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL:   sb_cstr(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: sb_number(b, v->num_val); return;
        case JSON_STRING: sb_pystr(b, v->str_val); return;
        case JSON_ARRAY:
            sb_ch(b, '[');
            for (i = 0; i < json_len(v); i++) {
                if (i) sb_cstr(b, ", ");
                sb_json(b, json_get(v, i));
            }
            sb_ch(b, ']');
            return;
        case JSON_OBJECT:
            sb_ch(b, '{');
            for (i = 0; i < json_object_size(v); i++) {
                const char *k = json_object_get_key_at(v, i);
                if (i) sb_cstr(b, ", ");
                sb_pystr(b, k ? k : "");
                sb_cstr(b, ": ");
                sb_json(b, json_object_get_at(v, i));
            }
            sb_ch(b, '}');
            return;
        default: sb_cstr(b, "null"); return;
    }
}

/* -- the JSONL speaker queue ------------------------------------------------- */

static int line_is_blank(const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f' && c != '\v') return 0;
    }
    return 1;
}

json_t *w19_openaiclie_7f783c_read_queue(const char *text, int exists, const char *uuid_value) {
    json_t *out = json_array();
    const char *p, *end;
    if (!exists || !text) return out;
    p = text;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
    end = p + strlen(p);
    while (p < end) {
        const char *nl = p;
        size_t len;
        json_t *entry, *id;
        while (nl < end && *nl != '\n' && *nl != '\r') nl++;
        len = (size_t)(nl - p);
        if (!line_is_blank(p, len)) {
            char *line = (char *)malloc(len + 1);
            if (line) {
                memcpy(line, p, len);
                line[len] = '\0';
                entry = json_parse(line, NULL);
                free(line);
                if (entry && entry->type == JSON_OBJECT) {
                    id = json_obj_get(entry, "id");
                    if (!id) json_set(entry, "id", json_string(uuid_value ? uuid_value : ""));
                    json_append(out, entry);
                } else if (entry) {
                    json_free(entry);
                }
            }
        }
        if (nl < end && *nl == '\r') { nl++; if (nl < end && *nl == '\n') nl++; }
        else if (nl < end) nl++;
        p = nl;
    }
    return out;
}

char *w19_openaiclie_7f783c_rewrite_queue(const json_t *entries) {
    sb_t b;
    char *res;
    memset(&b, 0, sizeof b);
    if (entries && entries->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(entries); i++) {
            sb_json(&b, json_get(entries, i));
            sb_ch(&b, '\n');
        }
    }
    res = b.buf ? b.buf : dupstr("");
    return res;
}

char *w19_openaiclie_7f783c_append_processed(const json_t *entry, const json_t *result) {
    json_t *record = json_object();
    sb_t b;
    char *res;
    const json_t *id = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "id") : NULL;
    const json_t *text = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "text") : NULL;
    json_set(record, "id", id ? json_copy(id) : json_null());
    json_set(record, "text", text ? json_copy(text) : json_string(""));
    json_set(record, "result", result ? json_copy(result) : json_null());
    memset(&b, 0, sizeof b);
    sb_json(&b, record);
    sb_ch(&b, '\n');
    res = b.buf ? b.buf : dupstr("");
    json_free(record);
    return res;
}

