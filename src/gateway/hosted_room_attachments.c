/*
 * hosted_room_attachments.c - C11 port of upstream Hermes gateway/hosted_room_attachments.py
 * (gate w23_hostedroom_e2fcc7).  The storage repository, the private byte directory, the room
 * log and the wall clock are seams; the argument checks, the magic-byte MIME sniff and the
 * bounded base64 RPC codec are ported here and differentially verified against live Python.
 * PoP: gateway_hosted_room_attachments_pending @ gateway/hosted_room_attachments.py:<module>
 * PoP: w23_hostedroom_e2fcc7_identifier @ gateway/hosted_room_attachments.py:_identifier
 * PoP: w23_hostedroom_e2fcc7_sniff_mime @ gateway/hosted_room_attachments.py:_sniff_known_mime
 * PoP: w23_hostedroom_e2fcc7_validate_mime_kind @ gateway/hosted_room_attachments.py:_validate_mime_and_kind
 * PoP: w23_hostedroom_e2fcc7_decode_b64 @ gateway/hosted_room_attachments.py:decode_content_base64
 * PoP: w23_hostedroom_e2fcc7_encode_b64 @ gateway/hosted_room_attachments.py:encode_content_base64
 * PoP: w23_hostedroom_e2fcc7_consts @ gateway/hosted_room_attachments.py:<module constants>
 */
#include <stddef.h>

void gateway_hosted_room_attachments_port_pending(void) { (void)0; }

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_hostedroom_e2fcc7.h"

/* ---- result builders ---------------------------------------------------- */

static json_t *hr_ok(json_t *value) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", value ? value : json_null());
    return o;
}

static json_t *hr_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}

/* "<kind> <label>" - the two message shapes _identifier raises. */
static json_t *hr_err2(const char *kind, const char *label) {
    char buf[256];
    snprintf(buf, sizeof buf, "%s %s", kind, label ? label : "");
    return hr_err(buf);
}

/* ---- bytes -------------------------------------------------------------- */

typedef struct {
    unsigned char *p;
    size_t n;
} hr_bytes;

static const char hr_alpha[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int hr_alpha_index(unsigned char c) {
    const char *hit;
    if (c == '\0') return -1;
    hit = strchr(hr_alpha, (int)c);
    return hit ? (int)(hit - hr_alpha) : -1;
}

/* Strict RFC 4648 decode, the shape base64.b64decode(v, validate=True) has:
 * length a multiple of four, only alphabet bytes plus at most two trailing
 * '=', nothing after the padding. */
static int hr_b64_strict(const char *s, size_t len, hr_bytes *out) {
    size_t i, pads = 0, chars, o = 0;
    unsigned char *buf;

    out->p = NULL;
    out->n = 0;
    if (!s || len == 0 || (len % 4) != 0) return 0;
    for (i = 0; i < len; i++) {
        if (s[i] == '=') {
            pads++;
            if (i + 1 < len && s[i + 1] != '=') return 0;
        } else if (hr_alpha_index((unsigned char)s[i]) < 0) {
            return 0;
        } else if (pads) {
            return 0; /* data after padding */
        }
    }
    if (pads > 2) return 0;
    chars = len - pads;
    buf = (unsigned char *)malloc(chars * 3 / 4 + 1);
    if (!buf) return 0;
    for (i = 0; i + 4 <= len; i += 4) {
        int a = hr_alpha_index((unsigned char)s[i]);
        int b = hr_alpha_index((unsigned char)s[i + 1]);
        int c = (i + 2 < chars) ? hr_alpha_index((unsigned char)s[i + 2]) : -2;
        int d = (i + 3 < chars) ? hr_alpha_index((unsigned char)s[i + 3]) : -2;
        uint32_t v;
        if (a < 0 || b < 0 || c == -1 || d == -1) { free(buf); return 0; }
        v = ((uint32_t)a << 18) | ((uint32_t)b << 12) |
            ((uint32_t)(c < 0 ? 0 : c) << 6) | (uint32_t)(d < 0 ? 0 : d);
        buf[o++] = (unsigned char)((v >> 16) & 0xFF);
        if (c >= 0) buf[o++] = (unsigned char)((v >> 8) & 0xFF);
        if (d >= 0) buf[o++] = (unsigned char)(v & 0xFF);
    }
    out->p = buf;
    out->n = o;
    return 1;
}

static char *hr_b64_encode(const unsigned char *data, size_t len) {
    size_t i, o = 0, need = ((len + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    for (i = 0; i + 2 < len; i += 3) {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) |
                     (uint32_t)data[i + 2];
        out[o++] = hr_alpha[(v >> 18) & 0x3F];
        out[o++] = hr_alpha[(v >> 12) & 0x3F];
        out[o++] = hr_alpha[(v >> 6) & 0x3F];
        out[o++] = hr_alpha[v & 0x3F];
    }
    if (i < len) {
        uint32_t v = (uint32_t)data[i] << 16;
        int rest = (int)(len - i);
        if (rest == 2) v |= (uint32_t)data[i + 1] << 8;
        out[o++] = hr_alpha[(v >> 18) & 0x3F];
        out[o++] = hr_alpha[(v >> 12) & 0x3F];
        out[o++] = (rest == 2) ? hr_alpha[(v >> 6) & 0x3F] : '=';
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

/* The fixture carries raw bytes as base64; that framing is the transport, not
 * the subject, so it decodes leniently (canonical input, any padding shape). */
static int hr_bytes_from_fixture(const json_t *req, const char *key, hr_bytes *out) {
    const char *s = json_get_str(req, key, "");
    size_t len = strlen(s), i, o = 0;
    char *canon;
    int ok;

    out->p = NULL;
    out->n = 0;
    if (!len) return 1;
    canon = (char *)malloc(len + 4);
    if (!canon) return 0;
    for (i = 0; i < len; i++) {
        if (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t') continue;
        if (s[i] == '=') continue;
        canon[o++] = s[i];
    }
    while ((o % 4) != 0) canon[o++] = '=';
    canon[o] = '\0';
    ok = hr_b64_strict(canon, o, out);
    free(canon);
    return ok;
}

/* Strict UTF-8 (no overlongs, no surrogates, nothing past U+10FFFF). */
static int hr_utf8_valid(const unsigned char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = s[i];
        if (c < 0x80) { i++; continue; }
        if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= n || (s[i + 1] & 0xC0) != 0x80 || c < 0xC2) return 0;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            uint32_t cp;
            if (i + 2 >= n || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80)
                return 0;
            cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[i + 1] & 0x3F) << 6) |
                 (uint32_t)(s[i + 2] & 0x3F);
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            uint32_t cp;
            if (i + 3 >= n || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80 ||
                (s[i + 3] & 0xC0) != 0x80)
                return 0;
            cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[i + 1] & 0x3F) << 12) |
                 ((uint32_t)(s[i + 2] & 0x3F) << 6) | (uint32_t)(s[i + 3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) return 0;
            i += 4;
        } else {
            return 0;
        }
    }
    return 1;
}

static int hr_has_prefix(const hr_bytes *b, const char *pfx) {
    size_t n = strlen(pfx);
    if (!b->p || b->n < n) return 0;
    return memcmp(b->p, pfx, n) == 0;
}

static int hr_has_byte(const hr_bytes *b, unsigned char byte) {
    size_t i;
    for (i = 0; i < b->n; i++)
        if (b->p[i] == byte) return 1;
    return 0;
}

/* ---- the ported rules --------------------------------------------------- */

static int hr_is_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int hr_identifier_ok(const char *s) {
    size_t i, n;
    char c;
    if (!s || !*s) return 0;
    n = strlen(s);
    if (n > W23_HOSTEDROOM_E2FCC7_MAX_IDENTIFIER_CHARS) return 0;
    c = s[0];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        return 0;
    for (i = 1; i < n; i++) {
        c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == ':' || c == '-')
            continue;
        return 0;
    }
    return 1;
}

static const char *hr_sniff(const hr_bytes *data) {
    if (hr_has_prefix(data, "\x89PNG\r\n\x1a\n")) return "image/png";
    if (hr_has_prefix(data, "\xff\xd8\xff")) return "image/jpeg";
    if (hr_has_prefix(data, "GIF87a") || hr_has_prefix(data, "GIF89a")) return "image/gif";
    if (hr_has_prefix(data, "BM")) return "image/bmp";
    if (data->p && data->n >= 12 && memcmp(data->p, "RIFF", 4) == 0 &&
        memcmp(data->p + 8, "WEBP", 4) == 0)
        return "image/webp";
    if (hr_has_prefix(data, "%PDF-")) return "application/pdf";
    if (hr_has_prefix(data, "PK\x03\x04")) return "application/zip";
    if (hr_has_prefix(data, "\x1f\x8b")) return "application/gzip";
    if (!hr_has_byte(data, 0x00) && hr_utf8_valid(data->p, data->n)) return "text/plain";
    return "application/octet-stream";
}

static int hr_starts(const char *s, const char *pfx) {
    return s && strncmp(s, pfx, strlen(pfx)) == 0;
}

/* ---- exported surface --------------------------------------------------- */

json_t *w23_hostedroom_e2fcc7_identifier(const json_t *req) {
    const json_t *v;
    const char *label;
    const char *raw;
    size_t start = 0, end, n;
    char *norm;
    json_t *res;

    if (!req || req->type != JSON_OBJECT) return hr_err2("invalid", "room_id");
    label = json_get_str(req, "label", "room_id");
    v = json_obj_get(req, "value");
    if (!v || v->type != JSON_STRING) return hr_err2(label, "must be a string");

    raw = v->str_val ? v->str_val : "";
    end = strlen(raw);
    while (start < end && hr_is_space((unsigned char)raw[start])) start++;
    while (end > start && hr_is_space((unsigned char)raw[end - 1])) end--;
    if (end <= start) return hr_err2("invalid", label);
    n = end - start;
    norm = (char *)malloc(n + 1);
    if (!norm) return hr_err("invalid");
    memcpy(norm, raw + start, n);
    norm[n] = '\0';
    if (!hr_identifier_ok(norm)) {
        free(norm);
        return hr_err2("invalid", label);
    }
    res = hr_ok(json_string(norm));
    free(norm);
    return res;
}

json_t *w23_hostedroom_e2fcc7_sniff_mime(const json_t *req) {
    hr_bytes data;
    json_t *res;

    if (!hr_bytes_from_fixture(req, "data_b64", &data)) return hr_err("invalid data_b64");
    res = hr_ok(json_string(hr_sniff(&data)));
    free(data.p);
    return res;
}

json_t *w23_hostedroom_e2fcc7_validate_mime_kind(const json_t *req) {
    hr_bytes data;
    const char *kind;
    const char *mime;
    const char *sniffed;

    if (!req || req->type != JSON_OBJECT) return hr_err("invalid request");
    kind = json_get_str(req, "kind", "");
    mime = json_get_str(req, "mime", "");
    if (!hr_bytes_from_fixture(req, "data_b64", &data)) return hr_err("invalid data_b64");
    sniffed = hr_sniff(&data);
    free(data.p);

    if (!strcmp(kind, "image")) {
        if (!hr_starts(mime, "image/") || strcmp(sniffed, mime) != 0)
            return hr_err("image bytes do not match the declared MIME type");
        return hr_ok(json_null());
    }
    if (!strcmp(kind, "pdf")) {
        if (strcmp(mime, "application/pdf") != 0 ||
            strcmp(sniffed, "application/pdf") != 0)
            return hr_err("PDF bytes do not match application/pdf");
        return hr_ok(json_null());
    }
    if (hr_starts(mime, "image/") || !strcmp(mime, "application/pdf"))
        return hr_err("image and PDF MIME types require their matching kind");
    if (hr_starts(mime, "text/") && strcmp(sniffed, "text/plain") != 0)
        return hr_err("text bytes do not match the declared MIME type");
    /* Generic files are opaque bytes; ZIP/gzip are container formats many
     * non-archive MIME types use, so no magic/MIME agreement is required. */
    return hr_ok(json_null());
}

json_t *w23_hostedroom_e2fcc7_decode_b64(const json_t *req) {
    const json_t *raw;
    const char *value;
    size_t len, maximum_encoded, i;
    hr_bytes data;
    char *encoded;
    json_t *res;

    if (!req || req->type != JSON_OBJECT) return hr_err("content_base64 is required");
    raw = json_obj_get(req, "value");
    if (!raw || raw->type != JSON_STRING || !raw->str_val || !raw->str_val[0])
        return hr_err("content_base64 is required");
    value = raw->str_val;
    len = strlen(value);

    /* value.lstrip().lower().startswith("data:") */
    i = 0;
    while (i < len && hr_is_space((unsigned char)value[i])) i++;
    if (len - i >= 5) {
        static const char *pfx = "data:";
        int hit = 1;
        size_t k;
        for (k = 0; k < 5; k++) {
            unsigned char c = (unsigned char)value[i + k];
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
            if (c != (unsigned char)pfx[k]) { hit = 0; break; }
        }
        if (hit) return hr_err("data URLs are not accepted");
    }

    maximum_encoded = ((W23_HOSTEDROOM_E2FCC7_MAX_ATTACHMENT_BYTES + 2) / 3) * 4;
    if (len > maximum_encoded + 4)
        return hr_err("attachment exceeds the per-file size limit");

    if (!hr_b64_strict(value, len, &data))
        return hr_err("content_base64 is not valid base64");
    if (!data.n) {
        free(data.p);
        return hr_err("attachment must not be empty");
    }
    if (data.n > W23_HOSTEDROOM_E2FCC7_MAX_ATTACHMENT_BYTES) {
        free(data.p);
        return hr_err("attachment exceeds the per-file size limit");
    }
    encoded = hr_b64_encode(data.p, data.n);
    free(data.p);
    if (!encoded) return hr_err("content_base64 is not valid base64");
    res = hr_ok(json_string(encoded));
    free(encoded);
    return res;
}

json_t *w23_hostedroom_e2fcc7_encode_b64(const json_t *req) {
    hr_bytes data;
    char *encoded;
    json_t *res;

    if (!hr_bytes_from_fixture(req, "data_b64", &data)) return hr_err("invalid data_b64");
    encoded = hr_b64_encode(data.p, data.n);
    free(data.p);
    if (!encoded) return hr_err("invalid data_b64");
    res = hr_ok(json_string(encoded));
    free(encoded);
    return res;
}

json_t *w23_hostedroom_e2fcc7_consts(void) {
    json_t *o = json_object();
    json_set(o, "max_identifier_chars",
             json_number((double)W23_HOSTEDROOM_E2FCC7_MAX_IDENTIFIER_CHARS));
    json_set(o, "max_attachment_bytes",
             json_number((double)W23_HOSTEDROOM_E2FCC7_MAX_ATTACHMENT_BYTES));
    json_set(o, "max_attachments_per_message",
             json_number((double)W23_HOSTEDROOM_E2FCC7_MAX_ATTACHMENTS_PER_MESSAGE));
    json_set(o, "uncommitted_ttl_seconds",
             json_number((double)W23_HOSTEDROOM_E2FCC7_UNCOMMITTED_TTL_SECONDS));
    json_set(o, "disbanded_grace_seconds",
             json_number((double)W23_HOSTEDROOM_E2FCC7_DISBANDED_GRACE_SECONDS));
    json_set(o, "classic_attachment_ttl_seconds",
             json_number((double)W23_HOSTEDROOM_E2FCC7_CLASSIC_ATTACHMENT_TTL_SECONDS));
    return o;
}
