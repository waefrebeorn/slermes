/*
 * stream_consumer_redact.c — real C11 port of upstream Hermes
 *   gateway/stream_consumer_redact.py pure boundary helpers.
 * PoP: w18_streamcons_0914fb_probe_start @ gateway/stream_consumer_redact.py:_probe_start
 * PoP: w18_streamcons_0914fb_lookback_start @ gateway/stream_consumer_redact.py:_lookback_start
 *
 * Offsets are byte offsets; fixtures use ASCII text (the Python functions index
 * code points, which coincide with bytes for ASCII).
 */
#include "hermes_json.h"
#include "noc_w18_streamcons_0914fb.h"

#define TAIL_SCAN 512
#define LOOKBACK  1024

static int is_key_char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/* _NON_KEY_CHAR_RE.search(text, n-TAIL_SCAN, n) -> first non-key byte index. */
static long long first_non_key(const char *s, size_t n) {
    size_t lo = n > TAIL_SCAN ? n - TAIL_SCAN : 0;
    for (size_t i = lo; i < n; i++) {
        if (!is_key_char((unsigned char)s[i])) return (long long)i;
    }
    return -1;
}

long long w18_streamcons_0914fb_probe_start(const char *text, long long token_start) {
    size_t n = text ? strlen(text) : 0;
    if ((long long)n - token_start <= TAIL_SCAN) return token_start;
    return first_non_key(text, n);
}

static long long rfind_char(const char *s, char ch, long long lo, long long hi) {
    long long found = -1;
    for (long long i = lo; i < hi; i++) {
        if (s[i] == ch) found = i;
    }
    return found;
}

long long w18_streamcons_0914fb_lookback_start(const char *text, long long upto) {
    size_t n = text ? strlen(text) : 0;
    long long hi = upto;
    if (hi > (long long)n) hi = (long long)n;
    if (hi < 0) hi = 0;
    long long lo = upto - LOOKBACK;
    if (lo < 0) lo = 0;
    if (lo > hi) lo = hi;
    long long newline = rfind_char(text ? text : "", '\n', lo, hi);
    if (newline >= 0 || lo == 0) return newline + 1;
    long long sp = rfind_char(text, ' ', 0, lo);
    long long tab = rfind_char(text, '\t', 0, lo);
    return (sp > tab ? sp : tab) + 1;
}
