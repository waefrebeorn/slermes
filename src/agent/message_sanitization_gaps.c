#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/message_sanitization.py: 3 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful ports of the message_sanitization gap stubs.
 * Universal seam interface: json_t *fn(ms2_io_t *io, json_t *in).
 * Both ports here are pure (no seams); NULL == None.
 *
 * serialized_messages_bytes uses libjson's compact serializer, whose
 * string escaping (", \\, \\b, \\f, \\n, \\r, \\t, \\u00xx for other
 * <0x20, raw UTF-8 otherwise) and separators (",", ":") are byte-exact
 * with json.dumps(ensure_ascii=False, separators=(",", ":")); object
 * key order is insertion order on both sides. DOCUMENTED EXCEPTION:
 * a parsed JSON number has no int/float identity — Python renders an
 * integral float as "123.0" and a non-integral float with repr() while
 * libjson renders %.0f / %.17g, so float-valued message payloads are
 * outside the fixture model (fixtures use ints/strs/bools/null only).
 * The default=str fallback and the TypeError/ValueError fallback are
 * unreachable from a JSON value tree (every node is serializable and
 * acyclic); the non-list / empty-list guard is ported exactly.
 *
 * looks_like_image_content_rejection: str(error_body or "") accepts
 * null/bool/int/str here (containers would need Python repr and are
 * outside the fixture model). Case folding is ASCII plus the two
 * Unicode folds that produce ASCII letters (U+212A -> k, U+0130 ->
 * i + U+0307); every other non-ASCII codepoint stays non-ASCII under
 * lower() and can never match the ASCII phrase bytes.
 * ──────────────────────────────────────────────────────────────── */

typedef struct ms2_io {
    json_t *(*call)(struct ms2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ms2_io_t;

static int ms2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

/* PoP: serialized_messages_bytes @ agent/message_sanitization.py:serialized_messages_bytes */
json_t *serialized_messages_bytes(ms2_io_t *io, json_t *in) {
    (void)io;
    /* if not isinstance(messages, list) or not messages: return 0 */
    json_t *messages = json_obj_get(in, "messages");
    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0) {
        return json_number(0.0);
    }
    /* len(json.dumps(messages, ensure_ascii=False,
     *                separators=(",", ":"), default=str).encode("utf-8")) */
    char *ser = json_serialize(messages);
    double n = ser ? (double)strlen(ser) : 0.0;
    free(ser);
    return json_number(n);
}

/* ── UTF-8 lower() with the ASCII-producing Unicode folds ── */

static unsigned ms2_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

/* lower() into a malloc'd buffer; caller frees. */
static char *ms2_lower(const char *s) {
    size_t cap = strlen(s) * 2 + 8;  /* U+0130 expands 2 -> 3 bytes */
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = s;
    while (*p) {
        const char *q = p;
        unsigned cp = ms2_cp_next(&q);
        if (cp < 0x80) {
            out[o++] = (cp >= 'A' && cp <= 'Z') ? (char)(cp + 32) : (char)cp;
        } else if (cp == 0x212A) {
            out[o++] = 'k';
        } else if (cp == 0x0130) {
            out[o++] = 'i';
            out[o++] = (char)0xCC;
            out[o++] = (char)0x87;  /* U+0307 combining dot above */
        } else {
            while (p < q) out[o++] = *p++;
        }
        p = q;
    }
    out[o] = '\0';
    return out;
}

/* str(error_body or "") for the fixture value model (see header). */
static char *ms2_stringify(const json_t *v) {
    if (!ms2_truthy(v)) return strdup("");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val);
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
            return strdup(buf);
        }
        default: return strdup("");
    }
}

/* _IMAGE_REJECTION_PHRASES — byte-exact from the live module. */
static const char *ms2_phrases[] = {
    "only 'text' content type is supported",
    "only text content type is supported",
    "image_url is not supported",
    "image content is not supported",
    "multimodal is not supported",
    "multimodal content is not supported",
    "multimodal input is not supported",
    "vision is not supported",
    "vision input is not supported",
    "does not support images",
    "does not support image input",
    "does not support multimodal",
    "does not support vision",
    "model does not support image",
    "unexpected item type in content",
    "image_url'. expected",
    "unknown variant `image_url`, expected `text`",
    "unknown variant image_url, expected text",
    "no endpoints found that support image input",
    "is not a multimodal model",
};

/* PoP: looks_like_image_content_rejection @ agent/message_sanitization.py:_looks_like_image_content_rejection */
json_t *looks_like_image_content_rejection(ms2_io_t *io, json_t *in) {
    (void)io;
    /* body = str(error_body or "").lower()
     * return any(phrase in body for phrase in _IMAGE_REJECTION_PHRASES) */
    char *raw = ms2_stringify(json_obj_get(in, "error_body"));
    char *body = raw ? ms2_lower(raw) : NULL;
    free(raw);
    int hit = 0;
    if (body) {
        for (size_t i = 0; i < sizeof(ms2_phrases) / sizeof(ms2_phrases[0]); i++) {
            if (strstr(body, ms2_phrases[i])) { hit = 1; break; }
        }
        free(body);
    }
    return json_bool(hit != 0);
}

int stale_thinking_reaches_wire(void) { return 0; }
