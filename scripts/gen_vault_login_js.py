#!/usr/bin/env python3
"""Generate src/cli/port_vault_login_js.c from the LIVE Python templates.

The two JS expressions are byte-level contracts (backslash escapes,
json.dumps spacing). Transcribing them by hand is how a port silently drifts,
so they are emitted from the module's own string constants.
"""
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import agent.vault_login_classifier as V  # noqa: E402


def c_literal(s: str) -> str:
    parts = s.split("\n")
    out = []
    for idx, line in enumerate(parts):
        esc = line.replace("\\", "\\\\").replace('"', '\\"')
        tail = "\\n" if idx < len(parts) - 1 else ""
        out.append('    "%s%s"' % (esc, tail))
    return "\n".join(out)


HEADER = r'''/*
 * port_vault_login_js.c — Port of Python: agent/vault_login_classifier.py
 * (the two JS builders).
 *
 * GENERATED FILE — do not hand-edit. `scripts/gen_vault_login_js.py` emits the
 * templates byte-for-byte from the LIVE Python module (backslashes and
 * json.dumps spacing are the contract). Regenerate after any Python change.
 */

#include "port_vault_login_classifier.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char INSPECTION_TEMPLATE[] =
'''

MIDDLE = r''';

static const char FILL_TEMPLATE[] =
'''

TAIL = r''';

/* Python json.dumps(str), ensure_ascii=True, no extra padding. */
static char *js_quote(const char *s)
{
    if (!s) s = "";
    size_t cap = strlen(s) * 8 + 8;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  out[o++] = '\\'; out[o++] = '"';  break;
        case '\\': out[o++] = '\\'; out[o++] = '\\'; break;
        case '\n': out[o++] = '\\'; out[o++] = 'n';  break;
        case '\r': out[o++] = '\\'; out[o++] = 'r';  break;
        case '\t': out[o++] = '\\'; out[o++] = 't';  break;
        case '\b': out[o++] = '\\'; out[o++] = 'b';  break;
        case '\f': out[o++] = '\\'; out[o++] = 'f';  break;
        default:
            if (*p < 0x20) {
                o += (size_t)snprintf(out + o, cap - o, "\\u%04x", *p);
            } else if (*p < 0x80) {
                out[o++] = (char)*p;
            } else {
                /* ensure_ascii: decode UTF-8 and emit \\uXXXX (surrogate pair
                 * for astral planes), exactly like json.dumps. */
                unsigned cp = 0;
                int extra = 0;
                if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
                else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
                else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
                for (int k = 1; k <= extra; k++)
                    cp = (cp << 6) | (p[k] & 0x3F);
                p += extra;              /* the for-loop advances the last byte */
                if (cp > 0xFFFF) {
                    unsigned v = cp - 0x10000;
                    o += (size_t)snprintf(out + o, cap - o, "\\u%04x\\u%04x",
                                          0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
                } else {
                    o += (size_t)snprintf(out + o, cap - o, "\\u%04x", cp);
                }
            }
        }
    }
    out[o++] = '"';
    out[o] = '\0';
    return out;
}

/* Replace every occurrence of `needle` with `with`. */
static char *replace_all(const char *text, const char *needle, const char *with)
{
    size_t nl = strlen(needle), wl = strlen(with);
    size_t cap = strlen(text) * 2 + 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = text;
    while (*p) {
        const char *hit = strstr(p, needle);
        size_t chunk = hit ? (size_t)(hit - p) : strlen(p);
        if (o + chunk + wl + 1 > cap) {
            cap = (o + chunk + wl + 1) * 2;
            char *tmp = (char *)realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
        memcpy(out + o, p, chunk);
        o += chunk;
        if (!hit) break;
        memcpy(out + o, with, wl);
        o += wl;
        p = hit + nl;
    }
    out[o] = '\0';
    return out;
}

/* PoP: vl_build_inspection_js @ agent/vault_login_classifier.py:build_inspection_js */
char *vl_build_inspection_js(const char *nonce)
{
    char *quoted = js_quote(nonce);
    if (!quoted) return NULL;
    char *out = replace_all(INSPECTION_TEMPLATE, "__NONCE__", quoted);
    free(quoted);
    return out;
}

/* PoP: vl_build_fill_js @ agent/vault_login_classifier.py:build_fill_js */
char *vl_build_fill_js(const char *fills_json, const char *expected_origin,
                       const char *nonce)
{
    json_t *fills = fills_json ? json_parse(fills_json, NULL) : NULL;
    if (!fills || fills->type != JSON_ARRAY) {
        if (fills) json_free(fills);
        return NULL;
    }

    /* payload = json.dumps([{"index": i, "token": t, "value": v}, ...]) with the
     * default separators and the key order the comprehension produces. */
    size_t cap = 64;
    for (size_t i = 0; i < fills->c.count; i++) {
        json_t *f = fills->c.items[i];
        const char *v = f ? json_get_str(f, "value", "") : "";
        const char *t = f ? json_get_str(f, "token", "") : "";
        cap += strlen(v) * 8 + strlen(t) * 8 + 96;
    }
    char *payload = (char *)malloc(cap);
    if (!payload) { json_free(fills); return NULL; }
    size_t o = 0;
    o += (size_t)snprintf(payload + o, cap - o, "[");
    for (size_t i = 0; i < fills->c.count; i++) {
        json_t *f = fills->c.items[i];
        json_t *idx = f ? json_obj_get(f, "index") : NULL;
        const char *declared = f ? json_get_str(f, "token", NULL) : NULL;
        const char *token = declared ? declared : "current-password";
        const char *value = f ? json_get_str(f, "value", "") : "";
        char *qt = js_quote(token);
        char *qv = js_quote(value);
        o += (size_t)snprintf(payload + o, cap - o,
                              "%s{\"index\": %ld, \"token\": %s, \"value\": %s}",
                              i ? ", " : "",
                              (long)(idx && idx->type == JSON_NUMBER ? idx->num_val : 0),
                              qt ? qt : "\"\"", qv ? qv : "\"\"");
        free(qt);
        free(qv);
    }
    o += (size_t)snprintf(payload + o, cap - o, "]");
    json_free(fills);

    char *q_origin = js_quote(expected_origin);
    char *q_nonce = js_quote(nonce);
    if (!q_origin || !q_nonce) {
        free(q_origin); free(q_nonce); free(payload);
        return NULL;
    }
    char *step1 = replace_all(FILL_TEMPLATE, "__EXPECTED_ORIGIN__", q_origin);
    char *step2 = step1 ? replace_all(step1, "__FILLS__", payload) : NULL;
    char *out = step2 ? replace_all(step2, "__NONCE__", q_nonce) : NULL;
    free(q_origin); free(q_nonce); free(payload); free(step1); free(step2);
    return out;
}
'''


def main() -> int:
    body = (HEADER + c_literal(V._LOGIN_CONTROL_INSPECTION_JS_TEMPLATE)
            + MIDDLE + c_literal(V._FILL_JS_TEMPLATE) + TAIL)
    with open("src/cli/port_vault_login_js.c", "w", encoding="utf-8", newline="") as fh:
        fh.write(body)
    print(f"wrote src/cli/port_vault_login_js.c ({len(body)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
