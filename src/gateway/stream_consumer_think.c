/*
 * stream_consumer_think.c — C port of upstream Hermes gateway/stream_consumer_think.py.
 *
 * Mirrors the tag primitives bound from agent/think_scrubber.py (THINK_TAG_NAMES).
 *
 * PoP: w17_streamcons_bd185c_at_block_boundary @ gateway/stream_consumer_think.py:StreamThinkFilterMixin._at_block_boundary
 * PoP: w17_streamcons_bd185c_earliest_open_tag @ gateway/stream_consumer_think.py:StreamThinkFilterMixin._earliest_open_tag
 * PoP: w17_streamcons_bd185c_strip_orphan_close_tags @ gateway/stream_consumer_think.py:StreamThinkFilterMixin._strip_orphan_close_tags
 */
#include "noc_w17_streamcons_bd185c.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

/* THINK_OPEN_TAGS / THINK_CLOSE_TAGS from agent/think_scrubber.py (names lowercased). */
static const char *OPEN_TAGS[10] = {
    "<think>", "<thinking>", "<reasoning>", "<thought>", "<reasoning_scratchpad>",
    "<思考>", "<反思>", "<推理>", "<推敲>", "<mm:think>"
};
static const char *CLOSE_TAGS[10] = {
    "</think>", "</thinking>", "</reasoning>", "</thought>", "</reasoning_scratchpad>",
    "</思考>", "</反思>", "</推理>", "</推敲>", "</mm:think>"
};

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* Count UTF-8 codepoints (lead bytes) in the first n bytes. */
static size_t cp_count_n(const char *s, size_t n) {
    size_t c = 0;
    for (size_t i = 0; i < n; i++)
        if (((unsigned char)s[i] & 0xC0) != 0x80) c++;
    return c;
}
static size_t cp_count(const char *s) { return cp_count_n(s, strlen(s)); }

/* Byte offset of the idx-th codepoint: always a UTF-8 lead byte (or strlen at end). */
static size_t cp_byte_off(const char *s, size_t idx) {
    size_t c = 0, i = 0;
    while (s[i]) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {  /* at a UTF-8 lead byte */
            if (c == idx) return i;
            c++;
        }
        i++;
    }
    return i;
}

/* ASCII-lowercase copy; byte length is preserved (CJK bytes untouched). */
static char *ascii_lower_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';
    return out;
}

static int is_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static int all_ws(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (!is_py_ws(s[i])) return 0;
    return 1;
}

int w17_streamcons_bd185c_at_block_boundary(const char *accumulated, const char *buf, int idx) {
    const char *acc = accumulated ? accumulated : "";
    const char *b = buf ? buf : "";
    size_t al = strlen(acc);
    int acc_boundary = (al == 0) || (acc[al - 1] == '\n');
    if (idx == 0) return acc_boundary;

    size_t off = cp_byte_off(b, (size_t)idx);   /* preceding = b[:idx] */
    size_t last_nl = (size_t)-1;
    for (size_t i = 0; i < off; i++)
        if (b[i] == '\n') last_nl = i;
    if (last_nl == (size_t)-1)
        return acc_boundary && all_ws(b, off);  /* preceding.strip() == "" */
    return all_ws(b + last_nl + 1, off - last_nl - 1);  /* line after last newline is blank */
}

void w17_streamcons_bd185c_earliest_open_tag(const char *accumulated, const char *buf,
        int *out_idx, int *out_len) {
    *out_idx = -1;
    *out_len = 0;
    const char *b = buf ? buf : "";
    char *lower = ascii_lower_dup(b);
    if (!lower) return;
    int best = -1, bestlen = 0;
    for (int t = 0; t < 10; t++) {
        const char *search = lower;
        for (;;) {
            const char *p = strstr(search, OPEN_TAGS[t]);
            if (!p) break;
            size_t boff = (size_t)(p - lower);
            int idx_cp = (int)cp_count_n(lower, boff);   /* char index, not byte offset */
            if (w17_streamcons_bd185c_at_block_boundary(accumulated, b, idx_cp)) {
                int tlen = (int)cp_count(OPEN_TAGS[t]);  /* char length (CJK aware) */
                if (best == -1 || idx_cp < best) { best = idx_cp; bestlen = tlen; }
                break;  /* first boundary hit for this tag is enough */
            }
            search = p + 1;  /* search_start = idx + 1 (one character past the '<') */
        }
    }
    free(lower);
    *out_idx = best;
    *out_len = bestlen;
}

/* Case-insensitive (ASCII) match of tag at s. */
static int match_tag_at(const char *s, const char *tag, size_t tl) {
    for (size_t i = 0; i < tl; i++) {
        char a = s[i], b = tag[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

char *w17_streamcons_bd185c_strip_orphan_close_tags(const char *text) {
    const char *s = text ? text : "";
    if (!strstr(s, "</")) return dupstr(s);
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return dupstr("");
    size_t k = 0, i = 0;
    while (i < n) {
        int matched = 0;
        for (int t = 0; t < 10; t++) {
            size_t tl = strlen(CLOSE_TAGS[t]);
            if (i + tl <= n && match_tag_at(s + i, CLOSE_TAGS[t], tl)) {
                i += tl;
                while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
                matched = 1;
                break;
            }
        }
        if (!matched) out[k++] = s[i++];
    }
    out[k] = '\0';   /* write NUL at the shortened length */
    return out;
}
