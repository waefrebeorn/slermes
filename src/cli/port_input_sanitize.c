/* port_input_sanitize.c — faithful C11 port of hermes_cli/input_sanitize.py
 *
 * Pure string sanitization of leaked terminal/paste control sequences.
 * Reuses nothing exotic: CRT strstr for unconditional removes; a manual
 * boundary scanner for the "remove [200~/01~ only at a boundary" rules
 * (avoids depending on regex backreference support). Faithful to Python's
 * regex semantics:
 *   START boundary: (^|[\s\n>:\]\)])[200~  -> keep boundary, drop [200~
 *   END   boundary: [201~(?=$|[\s\n<\[():;.,!?]) -> drop [200~/[201~
 *   (mirror for degraded 00~ / 01~)
 */

#include "input_sanitize.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: input_sanitize_strip @ hermes_cli/input_sanitize.py:strip_leaked_bracketed_paste_wrappers */
/* Remove every occurrence of `needle` from `s`, returning a fresh string. */
static char *str_remove_all(const char *s, const char *needle) {
    if (!s) return NULL;
    size_t nlen = strlen(needle);
    if (nlen == 0) return strdup(s);
    size_t cap = strlen(s) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t oi = 0;
    const char *p = s;
    while (*p) {
        if (strncmp(p, needle, nlen) == 0) { p += nlen; continue; }
        out[oi++] = *p++;
    }
    out[oi] = '\0';
    return out;
}

/* True if `c` is a boundary char for the START rules. */
static int is_start_boundary(char c) {
    return c == '\0' || isspace((unsigned char)c) || c == '\n' ||
           c == '>' || c == ':' || c == ']' || c == ')' || c == '}';
}
/* True if `c` is a boundary char for the END rules. */
static int is_end_boundary(char c) {
    return c == '\0' || isspace((unsigned char)c) || c == '\n' ||
           c == '<' || c == '[' || c == '(' || c == ')' || c == ':' ||
           c == ';' || c == '.' || c == ',' || c == '!' || c == '?';
}

/* Remove `marker` (e.g. "[200~") only when preceded by a START-boundary char
 * (keeping that boundary char) and followed by an END-boundary char (or EOL).
 * Returns a fresh string. */
static char *strip_boundary(const char *s, const char *marker) {
    if (!s) return NULL;
    size_t mlen = strlen(marker);
    size_t slen = strlen(s);
    size_t cap = slen + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < slen; ) {
        if (strncmp(s + i, marker, mlen) == 0) {
            char prev = (i == 0) ? '\0' : s[i - 1];
            char next = (i + mlen < slen) ? s[i + mlen] : '\0';
            if (is_start_boundary(prev) && is_end_boundary(next)) {
                i += mlen;            /* drop the marker */
                continue;
            }
        }
        out[oi++] = s[i++];
    }
    out[oi] = '\0';
    return out;
}

/* Remove `marker` (e.g. "[201~") only when followed by an END-boundary char
 * (or EOL), regardless of what precedes it. */
static char *strip_end_boundary(const char *s, const char *marker) {
    if (!s) return NULL;
    size_t mlen = strlen(marker);
    size_t slen = strlen(s);
    size_t cap = slen + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < slen; ) {
        if (strncmp(s + i, marker, mlen) == 0) {
            char next = (i + mlen < slen) ? s[i + mlen] : '\0';
            if (is_end_boundary(next)) {
                i += mlen;
                continue;
            }
        }
        out[oi++] = s[i++];
    }
    out[oi] = '\0';
    return out;
}

char *input_sanitize_strip_leaked_bracketed_paste_wrappers(const char *text) {
    if (!text || !*text) return text ? strdup(text) : NULL;

    /* Unconditional removes (Python's .replace chains). */
    char *s = str_remove_all(text, "\x1b[200~");
    char *t = str_remove_all(s, "\x1b[201~");
    free(s);
    s = str_remove_all(t, "^[[200~");
    free(t);
    t = str_remove_all(s, "^[[201~");
    free(s);

    /* Boundary rules (mirror the regexes exactly). */
    s = strip_boundary(t, "[200~");
    free(t);
    t = strip_end_boundary(s, "[201~");
    free(s);
    s = strip_boundary(t, "00~");
    free(t);
    t = strip_end_boundary(s, "01~");
    free(s);
    return t;
}

/* PoP: input_sanitize_collapse @ hermes_cli/input_sanitize.py:collapse_repeated_input_artifacts */
char *input_sanitize_collapse_repeated_input_artifacts(const char *text,
                                                       int min_repeats) {
    const char *marker = "~[[e";
    size_t mlen = strlen(marker);
    if (!text || !*text || mlen == 0) return text ? strdup(text) : NULL;

    size_t slen = strlen(text);
    long index = (long)slen;
    int repeat_count = 0;
    while (index >= (long)mlen &&
           strncmp(text + index - mlen, marker, mlen) == 0) {
        repeat_count++;
        index -= (long)mlen;
    }

    if (repeat_count < min_repeats) return strdup(text);

    long start = index;
    if (start >= 2 && strncmp(text + start - 2, "[e", 2) == 0) start -= 2;
    else if (start >= 1 && text[start - 1] == '[') start -= 1;

    char *out = malloc((size_t)start + 1);
    if (!out) return strdup(text);
    memcpy(out, text, (size_t)start);
    out[start] = '\0';
    return out;
}

/* PoP: input_sanitize_sanitize @ hermes_cli/input_sanitize.py:sanitize_user_prompt_text */
char *sanitize_user_prompt_text(const char *text) {
    if (!text || !*text) return text ? strdup(text) : NULL;
    char *cleaned = input_sanitize_strip_leaked_bracketed_paste_wrappers(text);
    char *result = input_sanitize_collapse_repeated_input_artifacts(cleaned, 4);
    free(cleaned);
    return result;
}
