/*
 * approval_git_options.c - C home for upstream Hermes tools/approval_git_options.py.
 * PoP: w14_approvalgi_b1c8ff_join_escaped_word_chars @ tools/approval_git_options.py:_join_escaped_word_chars
 *
 * Faithful port of the escaped-word joiner used by the destructive-git approval
 * grammar. The upstream implementation is a single re.sub over three alternatives
 * (single-quoted runs, double-quoted runs, and backslash+char), with a nested
 * re.sub(r'\\(.)', ...) inside each matched span. This is a linear scanner that
 * reproduces the same leftmost, non-overlapping match semantics.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_approvalgi_b1c8ff.h"

/* growable output buffer */
typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 1 >= b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    b->p[b->n++] = c;
}
static int in_set(char c, const char *set) {
    for (const char *p = set; *p; p++) if (*p == c) return 1;
    return 0;
}

/* nested re.sub(r'\\(.)', lambda m: '_' if m.group(1) in chars else m.group(0), span).
 * Python `.` does not match '\n', so a backslash+newline is left untouched. */
static void inner_join(buf_t *out, const char *span, size_t len, const char *chars) {
    size_t k = 0;
    while (k < len) {
        if (span[k] == '\\' && k + 1 < len && span[k + 1] != '\n') {
            char x = span[k + 1];
            if (in_set(x, chars)) bput(out, '_');
            else { bput(out, '\\'); bput(out, x); }
            k += 2;
        } else {
            bput(out, span[k]);
            k++;
        }
    }
}

struct json_t *w14_approvalgi_b1c8ff_join_escaped_word_chars(const char *command) {
    static const char *SEP = ";&|";
    static const char *DEF = ";&| \t\"'`";
    buf_t out = {0};
    const char *s = command ? command : "";
    size_t n = strlen(s), i = 0;

    while (i < n) {
        char c = s[i];
        if (c == '\'') {
            /* alternative 1: '[^']*' - needs a closing quote */
            size_t j = i + 1;
            while (j < n && s[j] != '\'') j++;
            if (j < n) {
                inner_join(&out, s + i, j - i + 1, SEP);
                i = j + 1;
                continue;
            }
            bput(&out, c); i++; continue;
        }
        if (c == '"') {
            /* alternative 2: "(?:[^"\\]|\\[\s\S])*" - needs a closing quote */
            size_t j = i + 1;
            int closed = 0;
            while (j < n) {
                if (s[j] == '\\') {
                    if (j + 1 < n) j += 2; else j += 1;
                } else if (s[j] == '"') {
                    closed = 1; break;
                } else {
                    j++;
                }
            }
            if (closed) {
                inner_join(&out, s + i, j - i + 1, DEF);
                i = j + 1;
                continue;
            }
            bput(&out, c); i++; continue;
        }
        if (c == '\\' && i + 1 < n) {
            /* alternative 3: \\[\s\S] */
            inner_join(&out, s + i, 2, DEF);
            i += 2;
            continue;
        }
        bput(&out, c); i++;
    }

    if (!out.p) out.p = (char *)calloc(1, 1);
    if (out.n) out.p[out.n] = 0; /* NUL terminator at the real length */
    json_t *r = json_string(out.p ? out.p : "");
    free(out.p);
    return r;
}
