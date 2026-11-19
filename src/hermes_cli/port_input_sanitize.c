/*
 * port_input_sanitize.c — Port of Python: hermes_cli/input_sanitize.py
 *
 * Prompt text leaked from terminal / desktop bracketed-paste control
 * sequences is scrubbed here. Pure ASCII byte surgery: the Python module
 * works on `str`, and every marker it matches is ASCII; UTF-8 payload bytes
 * pass through untouched (a multibyte sequence never contains an ASCII byte).
 *
 * Python's four sequential `re.sub` passes are reproduced as four sequential
 * scans, so a later pass sees the text produced by the earlier one — the
 * same left-to-right, non-overlapping semantics as `re.sub`.
 */

#include "port_input_sanitize.h"

#include <stdlib.h>
#include <string.h>

#define IS_WS(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r' || \
                  (c) == '\f' || (c) == '\v')

/* Python: (^|[\s\n>:\]\)]) — boundary BEFORE the marker. */
static int boundary_before(unsigned char c)
{
    return IS_WS(c) || c == '>' || c == ':' || c == ']' || c == ')';
}

/* Python: (?=$|[\s\n<\[\(\):;.,!?]) — boundary AFTER the marker. */
static int boundary_after(unsigned char c)
{
    return IS_WS(c) || c == '<' || c == '[' || c == '(' || c == ')' ||
           c == ':' || c == ';' || c == '.' || c == ',' || c == '!' ||
           c == '?';
}

static char *dup_or_null(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Drop `marker` where it sits at the start of the text or right after a
 * boundary character (pattern class A). */
static char *sub_marker_after_boundary(const char *s, const char *marker)
{
    size_t len = strlen(marker);
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0, i = 0;
    while (i < n) {
        int at_boundary = (i == 0) || boundary_before((unsigned char)s[i - 1]);
        if (at_boundary && i + len <= n && memcmp(s + i, marker, len) == 0) {
            i += len;
            continue;
        }
        out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}

/* Drop `marker` where the next character is end-of-text or a boundary
 * character (pattern class B). */
static char *sub_marker_before_boundary(const char *s, const char *marker)
{
    size_t len = strlen(marker);
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0, i = 0;
    while (i < n) {
        if (i + len <= n && memcmp(s + i, marker, len) == 0 &&
            (i + len == n || boundary_after((unsigned char)s[i + len]))) {
            i += len;
            continue;
        }
        out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}

/* Replace every occurrence of `needle` with "" (Python str.replace). */
static char *remove_all(const char *s, const char *needle)
{
    size_t len = strlen(needle);
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0, i = 0;
    while (i < n) {
        if (i + len <= n && memcmp(s + i, needle, len) == 0) {
            i += len;
            continue;
        }
        out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}

/* PoP: input_sanitize_strip_leaked_bracketed_paste_wrappers @ hermes_cli/input_sanitize.py:strip_leaked_bracketed_paste_wrappers */
char *input_sanitize_strip_leaked_bracketed_paste_wrappers(const char *text)
{
    static const char *const wrappers[] = {
        "\x1b[200~", "\x1b[201~", "^[[200~", "^[[201~"
    };
    if (!text) return NULL;

    char *cur = dup_or_null(text);
    if (!cur) return NULL;

    for (size_t w = 0; w < sizeof(wrappers) / sizeof(wrappers[0]); w++) {
        char *next = remove_all(cur, wrappers[w]);
        free(cur);
        if (!next) return NULL;
        cur = next;
    }

    /* Python's _BOUNDARY_SUBS order — 1,2,3,4 — is significant: each pass
     * sees the previous pass's output. */
    {
        static const struct { const char *marker; int before; } subs[] = {
            { "[200~", 1 },
            { "[201~", 0 },
            { "00~",   1 },
            { "01~",   0 },
        };
        for (size_t k = 0; k < sizeof(subs) / sizeof(subs[0]); k++) {
            char *next = subs[k].before
                       ? sub_marker_after_boundary(cur, subs[k].marker)
                       : sub_marker_before_boundary(cur, subs[k].marker);
            free(cur);
            if (!next) return NULL;
            cur = next;
        }
    }
    return cur;
}

/* PoP: input_sanitize_collapse_repeated_input_artifacts @ hermes_cli/input_sanitize.py:collapse_repeated_input_artifacts */
char *input_sanitize_collapse_repeated_input_artifacts(const char *text,
                                                       int min_repeats)
{
    /* Python default is min_repeats=4. */
    if (min_repeats <= 0) min_repeats = 4;
    if (!text) return NULL;

    static const char MARKER[] = "~[[e";
    const size_t mlen = sizeof(MARKER) - 1;
    size_t n = strlen(text);
    size_t index = n;
    int repeats = 0;

    while (index >= mlen && memcmp(text + index - mlen, MARKER, mlen) == 0) {
        repeats++;
        index -= mlen;
    }
    if (repeats < min_repeats) return dup_or_null(text);

    if (index >= 2 && memcmp(text + index - 2, "[e", 2) == 0) {
        index -= 2;
    } else if (index >= 1 && text[index - 1] == '[') {
        index -= 1;
    }

    char *out = (char *)malloc(index + 1);
    if (!out) return NULL;
    memcpy(out, text, index);
    out[index] = '\0';
    return out;
}

/* PoP: input_sanitize_sanitize_user_prompt_text @ hermes_cli/input_sanitize.py:sanitize_user_prompt_text */
char *input_sanitize_sanitize_user_prompt_text(const char *text)
{
    if (!text) return NULL;
    char *stripped = input_sanitize_strip_leaked_bracketed_paste_wrappers(text);
    if (!stripped) return NULL;
    char *collapsed = input_sanitize_collapse_repeated_input_artifacts(stripped, 4);
    free(stripped);
    return collapsed;
}
