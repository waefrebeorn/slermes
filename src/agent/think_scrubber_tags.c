/*
 * think_scrubber_tags.c — pure, argument-explicit ports of the think-tag
 * scanners in agent/think_scrubber.py.
 *
 * The scrubber in think_scrubber.c keeps its tag tables in module state; these
 * three helpers mirror the python signatures instead, so a gate can drive them
 * from argv:
 *
 *   ths_tag_find_first   @ StreamingThinkScrubber._find_first_tag(buf, tags)
 *   ths_tag_max_partial  @ StreamingThinkScrubber._max_partial_suffix(buf, tags)
 *   ths_strip_orphan     @ StreamingThinkScrubber._strip_orphan_close_tags(text)
 *
 * Matching is case-insensitive over ASCII, exactly like python's str.lower()
 * (CJK tag names are unaffected by case folding, so they compare bytewise).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* THINK_CLOSE_TAGS — must stay in sync with agent/think_scrubber.py. */
static const char *const CLOSE_TAGS[] = {
    "</" "think>", "</thinking>", "</reasoning>", "</thought>",
    "</reasoning_scratchpad>",
    "</\xE6\x80\x9D\xE8\x80\x83>",                 /* 思考 */
    "</\xE5\x8F\x8D\xE6\x80\x9D>",                 /* 反思 */
    "</\xE6\x8E\xA8\xE7\x90\x86>",                 /* 推理 */
    "</\xE6\x8E\xA8\xE6\x95\xB2>",                 /* 推敲 */
    "</mm:think>",
    NULL
};

static char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* case-insensitive find of `needle` in `hay`, starting at `from`; returns -1 or index */
static int find_ci(const char *hay, size_t hay_len, const char *needle, size_t from) {
    size_t nl = strlen(needle);
    if (!nl || nl > hay_len) return -1;
    for (size_t i = from; i + nl <= hay_len; i++) {
        size_t k = 0;
        while (k < nl && ascii_lower(hay[i + k]) == ascii_lower(needle[k])) k++;
        if (k == nl) return (int)i;
    }
    return -1;
}

/* PoP: ths_tag_find_first @ agent/think_scrubber.py:_find_first_tag
 * Earliest (index, tag_length) across *tags*, ties broken by shorter tag;
 * (-1, 0) when nothing matches. */
int ths_tag_find_first(const char *buf, const char *const *tags, int n, int *out_len) {
    if (out_len) *out_len = 0;
    if (!buf || !tags) return -1;
    size_t len = strlen(buf);
    int best_idx = -1, best_len = 0;
    for (int t = 0; t < n; t++) {
        const char *tag = tags[t];
        if (!tag || !tag[0]) continue;
        int idx = find_ci(buf, len, tag, 0);
        if (idx < 0) continue;
        int tl = (int)strlen(tag);
        if (best_idx < 0 || idx < best_idx || (idx == best_idx && tl < best_len)) {
            best_idx = idx; best_len = tl;
        }
    }
    if (out_len) *out_len = best_len;
    return best_idx;
}

/* PoP: ths_tag_max_partial @ agent/think_scrubber.py:_max_partial_suffix
 * Longest buf suffix that is a STRICT prefix of some tag (full matches belong to
 * the caller), bounded by max_tag_len - 1. */
long ths_tag_max_partial(const char *buf, const char *const *tags, int n, int max_tag_len) {
    if (!buf || !tags || max_tag_len <= 1) return 0;
    size_t blen = strlen(buf);
    size_t limit = (size_t)max_tag_len - 1;
    if (limit > blen) limit = blen;
    for (size_t i = limit; i > 0; i--) {
        const char *suffix = buf + blen - i;
        for (int t = 0; t < n; t++) {
            const char *tag = tags[t];
            if (!tag) continue;
            size_t tl = strlen(tag);
            if (tl <= i) continue;
            size_t k = 0;
            while (k < i && ascii_lower(tag[k]) == ascii_lower(suffix[k])) k++;
            if (k == i) return (long)i;
        }
    }
    return 0;
}

/* PoP: ths_strip_orphan @ agent/think_scrubber.py:_strip_orphan_close_tags
 * Drop close tags that have no matching open, together with the whitespace that
 * followed them (the python regex eats [ \t\n\r]*). Fast path mirrors python:
 * nothing to do unless "</" occurs. */
int ths_strip_orphan(const char *text, char *out, size_t cap) {
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (!text) return 0;
    size_t len = strlen(text);
    if (!strstr(text, "</")) {
        size_t n = len < cap - 1 ? len : cap - 1;
        memcpy(out, text, n); out[n] = 0;
        return (int)n;
    }
    size_t o = 0;
    for (size_t i = 0; i < len; ) {
        int matched = 0;
        if (text[i] == '<' && i + 1 < len && text[i + 1] == '/') {
            size_t tl = 0;
            for (int t = 0; CLOSE_TAGS[t]; t++) {
                size_t cl = strlen(CLOSE_TAGS[t]);
                if (i + cl > len) continue;
                size_t k = 0;
                while (k < cl && ascii_lower(text[i + k]) == ascii_lower(CLOSE_TAGS[t][k])) k++;
                if (k == cl && cl > tl) tl = cl;
            }
            if (tl) {
                i += tl;
                while (i < len && (text[i] == ' ' || text[i] == '\t' ||
                                   text[i] == '\n' || text[i] == '\r')) i++;
                matched = 1;
            }
        }
        if (!matched) {
            if (o + 1 < cap) out[o++] = text[i];
            i++;
        }
    }
    out[o] = 0;
    return (int)o;
}
