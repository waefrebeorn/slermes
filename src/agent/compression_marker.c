/*
 * compression_marker.c — C home for upstream Hermes agent/compression_marker.py.
 *
 * PoP: w10_compression_8beba6_elision_marker @ agent/compression_marker.py:_elision_marker
 * PoP: w10_compression_8beba6_elision_marker_max_len @ agent/compression_marker.py:ELISION_MARKER_MAX_LEN
 * PoP: w10_compression_8beba6_elide @ agent/compression_marker.py:elide
 * PoP: w10_compression_8beba6_elide_middle @ agent/compression_marker.py:elide_middle
 *
 * Dependency-free leaf: the marker template, its two renderers and the two
 * truncation helpers. Lengths are CODE POINTS (Python len/str-slice), not bytes,
 * because the ⟪/⟫ delimiters are multi-byte UTF-8.
 */
#include "noc_w10_compression_8beba6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python f"{value:,}" for a signed integer (right-grouped thousands). */
static char *fmt_count(long long value) {
    char raw[32];
    snprintf(raw, sizeof(raw), "%lld", value);
    int len = (int)strlen(raw);
    int sign = (raw[0] == '-') ? 1 : 0;
    int digits = len - sign;
    int commas = (digits > 0) ? (digits - 1) / 3 : 0;
    char *out = (char *)malloc((size_t)len + (size_t)commas + 1);
    if (!out) return xstrdup("");
    int oi = 0;
    if (sign) out[oi++] = '-';
    for (int i = 0; i < digits; i++) {
        if (i > 0 && (digits - i) % 3 == 0) out[oi++] = ',';
        out[oi++] = raw[sign + i];
    }
    out[oi] = '\0';
    return out;
}

/* Count UTF-8 code points (lead bytes, i.e. not continuation bytes). */
static size_t cp_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Advance one code point; returns the byte width. */
static size_t cp_adv(const char *p) {
    size_t a = 1;
    while (p[a] && ((unsigned char)p[a] & 0xC0) == 0x80) a++;
    return a;
}

/* First n code points of s, NUL-terminated (malloc'd). */
static char *cp_slice_n(const char *s, size_t n) {
    const char *p = s;
    size_t cnt = 0;
    while (*p && cnt < n) { p += cp_adv(p); cnt++; }
    size_t bytes = (size_t)(p - s);
    char *out = (char *)malloc(bytes + 1);
    if (!out) return xstrdup("");
    memcpy(out, s, bytes);
    out[bytes] = '\0';
    return out;
}

/* Last n code points of s (or all of s when it has fewer). malloc'd. */
static char *cp_tail_n(const char *s, size_t n) {
    size_t total = 0;
    for (const char *p = s; *p; p += cp_adv(p)) total++;
    if (total <= n) return xstrdup(s);
    const char *p = s;
    for (size_t i = 0; i < total - n; i++) p += cp_adv(p);
    return xstrdup(p);
}

/* Python str.rstrip() for ASCII whitespace (fixtures use ASCII text). */
static void rstrip_ws(char *s) {
    size_t n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') s[--n] = '\0';
        else break;
    }
}

char *w10_compression_8beba6_elision_marker(long long omitted, long long total) {
    char *o = fmt_count(omitted);
    char *t = fmt_count(total);
    size_t need = strlen(o) + strlen(t) + 256;
    char *buf = (char *)malloc(need);
    if (!buf) { free(o); free(t); return xstrdup(""); }
    snprintf(buf, need,
             "⟪HERMES-CONTEXT-COMPRESSION: %s of %s chars omitted here by Hermes's context compressor.⟫",
             o, t);
    free(o);
    free(t);
    return buf;
}

long long w10_compression_8beba6_elision_marker_max_len(void) {
    char *m = w10_compression_8beba6_elision_marker(999999999999LL, 999999999999LL);
    long long n = (long long)cp_len(m);
    free(m);
    return n;
}

char *w10_compression_8beba6_elide(const char *text, long long limit) {
    if (!text) text = "";
    size_t tlen = cp_len(text);
    if ((long long)tlen <= limit) return xstrdup(text);
    char *m = w10_compression_8beba6_elision_marker((long long)tlen, (long long)tlen);
    long long head_len = limit - (long long)cp_len(m);
    if (head_len <= 0) return m;
    char *kept = cp_slice_n(text, (size_t)head_len);
    rstrip_ws(kept);
    long long klen = (long long)cp_len(kept);
    free(m);
    m = w10_compression_8beba6_elision_marker((long long)tlen - klen, (long long)tlen);
    size_t outn = strlen(kept) + strlen(m) + 1;
    char *out = (char *)malloc(outn);
    if (!out) { free(kept); free(m); return xstrdup(""); }
    snprintf(out, outn, "%s%s", kept, m);
    free(kept);
    free(m);
    return out;
}

char *w10_compression_8beba6_elide_middle(const char *text, long long head, long long tail) {
    if (!text) text = "";
    size_t tlen = cp_len(text);
    if ((long long)tlen <= head + tail) return xstrdup(text);
    char *m = w10_compression_8beba6_elision_marker((long long)tlen - head - tail, (long long)tlen);
    char *h = cp_slice_n(text, (size_t)head);
    char *t = cp_tail_n(text, (size_t)tail);
    size_t outn = strlen(h) + strlen(m) + strlen(t) + 1;
    char *out = (char *)malloc(outn);
    if (!out) { free(h); free(t); free(m); return xstrdup(""); }
    snprintf(out, outn, "%s%s%s", h, m, t);
    free(h);
    free(t);
    free(m);
    return out;
}
