/*
 * approval_quoted_newlines.c — faithful C11 port of tools/approval_quoted_newlines.py.
 * Gate: appquote.
 * PoP: appquote_mask_quoted_newlines @ tools/approval_quoted_newlines.py:_mask_quoted_newlines
 * PoP: appquote_mask_quoted_newlines_span @ tools/approval_quoted_newlines.py:_mask_quoted_newlines_span
 *
 * Reimplements the subset of tools.approval_parser_budget._scan_shell used by
 * the masker: subst="q" (substitutions only inside double quotes), comments=True,
 * brace/stop_unterminated/naive_backtick off, no parser budget. Raw CR/LF read
 * as plain chars inside a quote are replaced by one space; substitutions are
 * re-scanned with a fresh quote state so a command boundary inside $(...) survives.
 */
#include "noc_appquote.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } sbuf;
static void sb_init(sbuf *b) { b->cap = 512; b->len = 0; b->p = (char *)malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void sb_grow(sbuf *b, size_t need) {
    if (b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap;
    while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}
static void sb_addn(sbuf *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    sb_grow(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void sb_add(sbuf *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static char *sb_take(sbuf *b) { char *p = b->p; b->p = NULL; b->len = b->cap = 0; return p; }

static int is_py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* text.startswith(pat, i), bounded by lim (not by the active span end). */
static int starts_at(const char *s, size_t i, const char *pat, size_t n, size_t lim) {
    if (i + n > lim) return 0;
    return strncmp(s + i, pat, n) == 0;
}

/* tools.approval_ansi_c._is_shell_comment_start over the whole command. */
static int is_shell_comment_start(const char *cmd, size_t idx) {
    if (cmd[idx] != '#') return 0;
    if (idx == 0) return 1;             /* command[index] == "#" */
    char prev = cmd[idx - 1];
    int ok = is_py_space((unsigned char)prev) ||
             prev == ';' || prev == '&' || prev == '|' || prev == '(' || prev == ')';
    if (!ok) return 0;
    size_t count = 0;
    if (idx >= 2) {
        size_t q = idx - 2;
        while (cmd[q] == '\\') { count++; if (q == 0) break; q--; }
    }
    return (count % 2) == 0;
}

/* _scan_backtick_end: regex (?:\\.|[^`\\])*` from start+1; offset past closer or -1. */
static long long scan_backtick_end(const char *cmd, size_t start) {
    size_t n = strlen(cmd);
    size_t k = start + 1;
    while (k < n) {
        if (cmd[k] == '\\') k += 2;              /* DOTALL: backslash escapes any char */
        else if (cmd[k] == '`') return (long long)(k + 1);
        else k++;
    }
    return -1;
}

/* _dollar_paren_end: balanced $(...) using _scan_shell(text, start+2) (no subst/comments). */
static long long scan_dollar_paren_end(const char *cmd, size_t start) {
    size_t n = strlen(cmd);
    size_t i = start + 2;
    int depth = 1, quote = 0;
    while (i < n) {
        char ch = cmd[i];
        int kind = 0;                    /* 0 char, 1 esc, 2 quote */
        size_t j = i + 1;
        if (quote != '\'' && ch == '\\' && i + 1 < n) { kind = 1; j = i + 2; }
        else if (ch == quote || (quote == 0 && (ch == '\'' || ch == '"'))) { kind = 2; }
        if (kind == 0 && quote == 0) {
            if (ch == '$' && i + 1 < n && cmd[i + 1] == '(') depth++;
            if (ch == ')') depth--;
            if (depth == 0) return (long long)(i + 1);
        }
        if (kind == 2) quote = quote ? 0 : ch;
        i = j;
    }
    return -1;
}

/* _mask_quoted_newlines_span over [start,end). */
static void mask_span(const char *cmd, long long start, long long end, sbuf *out) {
    size_t n = strlen(cmd);
    size_t i = (size_t)(start < 0 ? 0 : start);
    size_t e = (end < 0 || (size_t)end > n) ? n : (size_t)end;
    int quote = 0;
    while (i < e) {
        char ch = cmd[i];
        int kind = 0;                    /* 0 char, 1 esc, 2 quote, 3 subst, 4 comment */
        size_t j = i + 1;
        if (quote == 0 && is_shell_comment_start(cmd, i)) {
            kind = 4;
            const char *nl = memchr(cmd + i, '\n', e - i);
            j = nl ? (size_t)(nl - cmd) : e;
        } else if (quote != '\'' && ch == '\\' && i + 1 < e) {
            kind = 1; j = i + 2;
        } else if (ch == quote || (quote == 0 && (ch == '\'' || ch == '"'))) {
            kind = 2;
        } else if (quote == '"' && (ch == '`' || starts_at(cmd, i, "$(", 2, n))) {
            long long close = (ch == '`') ? scan_backtick_end(cmd, i)
                                          : scan_dollar_paren_end(cmd, i);
            if (close >= 0) { kind = 3; j = (size_t)close; }
        }

        if (kind == 3) {
            int dollar = starts_at(cmd, i, "$(", 2, n);
            size_t body_start = i + (dollar ? 2 : 1);
            size_t body_end = j - 1;
            sb_addn(out, cmd + i, body_start - i);
            mask_span(cmd, (long long)body_start, (long long)body_end, out);
            sb_addn(out, cmd + body_end, j - body_end);
        } else if (quote != 0 && kind == 0 && (ch == '\r' || ch == '\n')) {
            sb_addn(out, " ", 1);
        } else {
            sb_addn(out, cmd + i, j - i);
        }
        if (kind == 2) quote = quote ? 0 : ch;
        i = j;
    }
}

char *appquote_mask_quoted_newlines_span(const char *command, long long start, long long end) {
    if (!command) return NULL;
    sbuf out; sb_init(&out);
    mask_span(command, start, end, &out);
    return sb_take(&out);
}

char *appquote_mask_quoted_newlines(const char *command) {
    if (!command) return NULL;
    if (!strchr(command, '\n') && !strchr(command, '\r')) return strdup(command);
    sbuf out; sb_init(&out);
    mask_span(command, 0, (long long)strlen(command), &out);
    return sb_take(&out);
}
