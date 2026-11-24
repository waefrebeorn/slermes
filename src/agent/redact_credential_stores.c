/*
 * redact_credential_stores.c — C port of upstream Hermes agent/redact_credential_stores.py.
 *
 * PoP: w18_redactcred_108d32_aws_keys @ agent/redact_credential_stores.py:aws_credential_process_keys
 * PoP: w18_redactcred_108d32_outside @ agent/redact_credential_stores.py:outside_spans
 * PoP: w18_redactcred_108d32_constants @ agent/redact_credential_stores.py:_SECRET_CFG_NAMES/_LINE_NUMBER_GUTTER
 *
 * outside_spans' callback is a controlled seam instantiated as ASCII upper()
 * (the NUL-anchoring trick in the Python source is a no-op for upper()). The
 * value-masking callbacks of redact_credential_store_lines need agent.redact's
 * assignment gate and are out of scope.
 */
#include "noc_w18_redactcred_108d32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* _LINE_NUMBER_GUTTER = (?:[0-9]+(?:[|:\-]|\t)[ \t]*)? — optional, may not advance. */
static void gutter(const char *t, size_t lim, size_t *p) {
    size_t q = *p;
    while (q < lim && t[q] >= '0' && t[q] <= '9') q++;
    if (q > *p && q < lim && (t[q] == '|' || t[q] == ':' || t[q] == '-' || t[q] == '\t')) {
        q++;
        while (q < lim && (t[q] == ' ' || t[q] == '\t')) q++;
        *p = q;
    }
}

/* ^[ \t]*GUTTER\[[^\[\]\s][^\[\]\r\n]*\][ \t]*+$ on one line. */
static bool ini_section(const char *t, size_t len, size_t ls, size_t le, size_t *end) {
    size_t p = ls;
    while (p < le && (t[p] == ' ' || t[p] == '\t')) p++;
    gutter(t, le, &p);
    if (p >= le || t[p] != '[') return false;
    p++;
    if (p >= le) return false;
    unsigned char c = (unsigned char)t[p];
    if (c == '[' || c == ']' || isspace(c)) return false;
    p++;
    while (p < le) {
        unsigned char d = (unsigned char)t[p];
        if (d == '[' || d == ']' || d == '\r' || d == '\n') break;
        p++;
    }
    if (p >= le || t[p] != ']') return false;
    p++;
    while (p < le && (t[p] == ' ' || t[p] == '\t')) p++;
    if (p != le) return false;   /* $ anchors at end of line */
    *end = p;
    return true;
}

/* ^[ \t]*GUTTERcredential_process[ \t]*+=[ \t]*+ on one line. */
static bool cp_key(const char *t, size_t len, size_t ls, size_t le, size_t *start, size_t *end) {
    (void)len;
    size_t p = ls;
    while (p < le && (t[p] == ' ' || t[p] == '\t')) p++;
    gutter(t, le, &p);
    static const char kw[] = "credential_process";
    const size_t kl = sizeof(kw) - 1;
    if (p + kl > le || memcmp(t + p, kw, kl)) return false;
    p += kl;
    while (p < le && (t[p] == ' ' || t[p] == '\t')) p++;
    if (p >= le || t[p] != '=') return false;
    p++;
    while (p < le && (t[p] == ' ' || t[p] == '\t')) p++;
    *start = ls;
    *end = p;
    return true;
}

json_t *w18_redactcred_108d32_aws_keys(const char *text) {
    json_t *out = json_array();
    if (!text) return out;
    if (!strstr(text, "credential_process")) return out;
    size_t len = strlen(text), ls = 0;
    long long header_end = -1;
    /* _INI_SECTION_RE.search(text): first INI section line wins. */
    while (ls <= len) {
        size_t le = ls;
        while (le < len && text[le] != '\n') le++;
        size_t me;
        if (ini_section(text, len, ls, le, &me)) { header_end = (long long)me; break; }
        if (le >= len) break;
        ls = le + 1;
    }
    if (header_end < 0) return out;
    /* finditer(text, header.end()): only '^' line starts after the header. */
    size_t start = (size_t)header_end;
    if (start < len && text[start] == '\n') start++;
    while (start <= len) {
        size_t le = start;
        while (le < len && text[le] != '\n') le++;
        size_t a, b;
        if (cp_key(text, len, start, le, &a, &b)) {
            json_t *span = json_array();
            json_append(span, json_number((double)a));
            json_append(span, json_number((double)b));
            json_append(out, span);
        }
        if (le >= len) break;
        start = le + 1;
    }
    return out;
}

static size_t upper_range(const char *s, size_t from, size_t to, char *buf, size_t o) {
    const unsigned char *t = (const unsigned char *)s;
    size_t p = from;
    while (p < to) {
        unsigned char c = t[p];
        if (c >= 'a' && c <= 'z') { buf[o++] = (char)(c - 32); p++; continue; }
        /* Decode only from a UTF-8 LEAD byte; continuation bytes copy through. */
        if (c >= 0xC2 && c <= 0xDF && p + 1 < to && (t[p + 1] & 0xC0) == 0x80) {
            unsigned cp = ((unsigned)(c & 0x1F) << 6) | (t[p + 1] & 0x3F);
            if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) cp -= 32;  /* Latin-1 letter */
            buf[o++] = (char)(0xC0 | (cp >> 6));
            buf[o++] = (char)(0x80 | (cp & 0x3F));
            p += 2;
            continue;
        }
        buf[o++] = (char)c;
        p++;
    }
    return o;
}
static size_t upper_from(const char *s, size_t from, size_t to, char *buf, size_t o) {
    /* A tail must begin on a lead byte, never in the middle of a UTF-8 char. */
    const unsigned char *t = (const unsigned char *)s;
    while (from < to && (t[from] & 0xC0) == 0x80) from++;
    return upper_range(s, from, to, buf, o);
}

json_t *w18_redactcred_108d32_outside(const char *text, const json_t *spans) {
    if (!text) return json_string("");
    size_t len = strlen(text);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return json_string("");
    size_t o = 0, pos = 0;
    size_t ns = (spans && spans->type == JSON_ARRAY) ? json_len(spans) : 0;
    for (size_t i = 0; i < ns; i++) {
        json_t *s = json_get(spans, i);
        if (!s || s->type != JSON_ARRAY || json_len(s) < 2) continue;
        json_t *ia = json_get(s, 0), *ib = json_get(s, 1);
        long long a = (ia && (ia->type == JSON_NUMBER || ia->type == JSON_BOOL))
            ? (long long)(ia->type == JSON_BOOL ? (ia->bool_val ? 1 : 0) : ia->num_val) : 0;
        long long b = (ib && (ib->type == JSON_NUMBER || ib->type == JSON_BOOL))
            ? (long long)(ib->type == JSON_BOOL ? (ib->bool_val ? 1 : 0) : ib->num_val) : 0;
        if (a < 0) a = 0;
        if (b < 0) b = 0;
        if ((size_t)a > len) a = (long long)len;
        if ((size_t)b > len) b = (long long)len;
        if (b < a) b = a;
        if ((size_t)a < pos) a = (long long)pos;
        o = upper_from(text, pos, (size_t)a, buf, o);
        for (size_t j = (size_t)a; j < (size_t)b; j++) buf[o++] = text[j];
        pos = (size_t)b;
    }
    o = upper_from(text, pos, len, buf, o);
    buf[o] = '\0';   /* NUL at the new length */
    json_t *res = json_string(buf);
    free(buf);
    return res;
}

json_t *w18_redactcred_108d32_constants(void) {
    json_t *out = json_object();
    json_set(out, "_SECRET_CFG_NAMES", json_string("(?:api[ _.\\-]?key|token|secret|passwd|password|credential|auth)"));
    json_set(out, "_LINE_NUMBER_GUTTER", json_string("(?:[0-9]+(?:[|:\\-]|\\t)[ \\t]*)?"));
    return out;
}
