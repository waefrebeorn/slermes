/*
 * prompt_scan.c -- faithful C11 port of upstream Hermes cron/prompt_scan.py.
 *
 * PoP: w6_promptscan_439e13_strip_invisible_unicode @ cron/prompt_scan.py:_strip_invisible_unicode
 */
#include "noc_w6_promptscan_439e13.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- minimal UTF-8 codec over arrays of Unicode codepoints ---- */
static uint32_t *utf8_decode(const char *s, size_t *n) {
    size_t cap = strlen(s) + 1;
    uint32_t *cp = (uint32_t *)malloc(cap * sizeof(uint32_t));
    if (!cp) return NULL;
    size_t i = 0, k = 0;
    while (s[i]) {
        unsigned char c = (unsigned char)s[i];
        uint32_t v; int len;
        if (c < 0x80) { v = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { v = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { v = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { v = c & 0x07; len = 4; }
        else { v = c; len = 1; }
        for (int j = 1; j < len && s[i + j]; j++) v = (v << 6) | ((unsigned char)s[i + j] & 0x3F);
        cp[k++] = v; i += len;
    }
    *n = k;
    return cp;
}

static char *utf8_encode(const uint32_t *cp, size_t n) {
    size_t cap = n * 4 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t v = cp[i];
        if (v < 0x80) out[k++] = (char)v;
        else if (v < 0x800) { out[k++]=(char)(0xC0|(v>>6)); out[k++]=(char)(0x80|(v&0x3F)); }
        else if (v < 0x10000) { out[k++]=(char)(0xE0|(v>>12)); out[k++]=(char)(0x80|((v>>6)&0x3F)); out[k++]=(char)(0x80|(v&0x3F)); }
        else { out[k++]=(char)(0xF0|(v>>18)); out[k++]=(char)(0x80|((v>>12)&0x3F)); out[k++]=(char)(0x80|((v>>6)&0x3F)); out[k++]=(char)(0x80|(v&0x3F)); }
    }
    out[k] = '\0';
    return out;
}

static int invisible(uint32_t c) {
    if (c >= 0xE0000 && c <= 0xE007F) return 1;
    switch (c) {
    case 0x200B: case 0x200C: case 0x200D: case 0x2060:
    case 0x2062: case 0x2063: case 0x2064: case 0xFEFF:
    case 0x202A: case 0x202B: case 0x202C: case 0x202D: case 0x202E:
    case 0x2066: case 0x2067: case 0x2068: case 0x2069:
        return 1;
    default: return 0;
    }
}

static int emoji_cp(uint32_t c) {
    return (c >= 0x1F000 && c <= 0x1FFFF) || (c >= 0x2600 && c <= 0x27BF) ||
           (c >= 0x2300 && c <= 0x23FF) || (c >= 0x1F1E6 && c <= 0x1F1FF) ||
           c == 0x20E3;
}

/* _zwj_has_emoji_neighbour: skip VS16 on both sides, need emoji neighbours. */
static int zwj_has_emoji_neighbour(const uint32_t *cps, size_t n, size_t idx) {
    long left = (long)idx - 1;
    while (left >= 0 && cps[left] == 0xFE0F) left--;
    size_t right = idx + 1;
    while (right < n && cps[right] == 0xFE0F) right++;
    if (left < 0 || right >= n) return 0;
    return emoji_cp(cps[left]) && emoji_cp(cps[right]);
}

/* Is index i part of an RGI subdivision-flag tag sequence?
 * Sequence: U+1F3F4 + 5 tag chars (gbeng / gbsct / gbwls) + U+E007F.
 * Exempt positions are m.start()+1 .. m.end()-1 (5 tags + cancel tag). */
static int flag_tag(const uint32_t *cps, size_t n, size_t i) {
    static const char *codes[] = {"gbeng", "gbsct", "gbwls"};
    if (n < 7) return 0;
    for (size_t s = 0; s + 7 <= n; s++) {
        if (cps[s] != 0x1F3F4 || cps[s + 6] != 0xE007F) continue;
        int match = 0;
        for (size_t ci = 0; ci < 3; ci++) {
            int ok = 1;
            for (size_t k = 0; k < 5; k++)
                if (cps[s + 1 + k] != 0xE0000 + (uint32_t)(unsigned char)codes[ci][k]) { ok = 0; break; }
            if (ok) { match = 1; break; }
        }
        if (match && i >= s + 1 && i <= s + 6) return 1;
    }
    return 0;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* PoP: w6_promptscan_439e13_strip_invisible_unicode @ cron/prompt_scan.py:_strip_invisible_unicode */
json_t *w6_promptscan_439e13_strip_invisible_unicode(const char *prompt) {
    json_t *out = json_object();
    if (!out) return NULL;
    json_t *removed = json_array();
    json_set(out, "cleaned", json_string(prompt ? prompt : ""));
    json_set(out, "removed", removed);
    if (!prompt || !*prompt) return out;

    size_t n = 0;
    uint32_t *cps = utf8_decode(prompt, &n);
    if (!cps) return out;
    uint32_t *kept = (uint32_t *)malloc((n + 1) * sizeof(uint32_t));
    char **labels = NULL;
    size_t nlabels = 0, lcap = 0;
    size_t k = 0;

    for (size_t i = 0; i < n; i++) {
        uint32_t c = cps[i];
        if (!flag_tag(cps, n, i) && invisible(c) &&
            !(c == 0x200D && zwj_has_emoji_neighbour(cps, n, i))) {
            char buf[16];
            snprintf(buf, sizeof(buf), "U+%04X", c);
            int dup = 0;
            for (size_t j = 0; j < nlabels; j++) if (!strcmp(labels[j], buf)) { dup = 1; break; }
            if (!dup) {
                if (nlabels == lcap) {
                    lcap = lcap ? lcap * 2 : 8;
                    labels = (char **)realloc(labels, lcap * sizeof(char *));
                }
                labels[nlabels++] = strdup(buf);
            }
            continue;
        }
        kept[k++] = c;
    }

    char *cleaned = utf8_encode(kept, k);
    json_set(out, "cleaned", json_string(cleaned ? cleaned : ""));
    free(cleaned);
    if (nlabels > 1) qsort(labels, nlabels, sizeof(char *), cmp_str);
    for (size_t j = 0; j < nlabels; j++) {
        json_append(removed, json_string(labels[j]));
        free(labels[j]);
    }
    free(labels);
    free(kept);
    free(cps);
    return out;
}
