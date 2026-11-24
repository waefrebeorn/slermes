/*
 * approval_parser_budget.c - C home for upstream Hermes tools/approval_parser_budget.py.
 * PoP: w16_approvalpa_cd9d4d_substitution_depth @ tools/approval_parser_budget.py:substitution_depth
 * PoP: w16_approvalpa_cd9d4d_scan_shell @ tools/approval_parser_budget.py:_scan_shell
 * PoP: w16_approvalpa_cd9d4d_dollar_paren_end @ tools/approval_parser_budget.py:_scan_dollar_paren_end
 * PoP: w16_approvalpa_cd9d4d_backtick_end @ tools/approval_parser_budget.py:_scan_backtick_end
 * PoP: w16_approvalpa_cd9d4d_heredoc_bodies @ tools/approval_parser_budget.py:_quoted_heredoc_body_spans
 *
 * Pure scanner helpers only. Runtime budget accounting (contextvars, locks, memo) stays in
 * Python: every exported helper here is the budget=active() is None path. Input is UTF-8;
 * offsets are byte offsets, so callers should keep fixtures ASCII for Python index parity.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_approvalpa_cd9d4d.h"

static char *dupn(const char *s, size_t n) { char *p = (char *)malloc(n + 1); if (p) { if (n) memcpy(p, s, n); p[n] = 0; } return p; }
static long long slen(const char *s) { return s ? (long long)strlen(s) : 0; }
static double node_num(const json_t *n) { return n && n->type == JSON_NUMBER ? n->num_val : 0.0; }

typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 1 >= b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; char *np = (char *)realloc(b->p, nc); if (!np) return; b->p = np; b->cap = nc; }
    b->p[b->n++] = c;
}
static char *bdone(buf_t *b) { if (!b->p) b->p = (char *)calloc(1, 1); if (b->p) b->p[b->n] = 0; return b->p; }

/* Python str.find(c, from) over the whole string; -1 when absent. */
static long long find_char(const char *s, char c, size_t from) {
    size_t n = strlen(s);
    if (from > n) return -1;
    const char *p = strchr(s + from, c);
    return p ? (long long)(p - s) : -1;
}

/* tools/approval_ansi_c._is_shell_comment_start (the only import of the live module). */
static int is_shell_comment_start(const char *cmd, size_t idx) {
    if (cmd[idx] != '#') return cmd[idx] == '#';
    if (idx == 0) return 1;
    char prev = cmd[idx - 1];
    if (!(isspace((unsigned char)prev) || strchr(";&|()<>", prev))) return 0;
    long long j = (long long)idx - 2;
    while (j >= 0 && cmd[j] == '\\') j--;
    return ((long long)idx - 2 - j) % 2 == 0;
}

/* ---- _HEREDOC_RE: <<(-?)[ \t]*((?:'[^'\n]*'|"[^"\n]*"|\\.|[^ \s;&|<>()'"\\])+) ---- */
static long long match_heredoc(const char *cmd, size_t i, char **delim_out, int *strip_out, int *quoted_out) {
    size_t n = strlen(cmd);
    if (i + 1 >= n || cmd[i] != '<' || cmd[i + 1] != '<') return -1;
    size_t j = i + 2;
    int strip = 0, quoted = 0;
    if (j < n && cmd[j] == '-') { strip = 1; j++; }
    while (j < n && (cmd[j] == ' ' || cmd[j] == '\t')) j++;
    buf_t word = {0};
    while (j < n) {
        char c = cmd[j];
        if (c == '\'' || c == '"') {
            char q = c; size_t k = j + 1;
            while (k < n && cmd[k] != q && cmd[k] != '\n') k++;
            if (k >= n || cmd[k] != q) break;
            for (size_t t = j; t <= k; t++) bput(&word, cmd[t]);
            j = k + 1; quoted = 1; continue;
        }
        if (c == '\\') {
            if (j + 1 >= n || cmd[j + 1] == '\n') break;  /* re.DOTALL is off */
            bput(&word, c); bput(&word, cmd[j + 1]); j += 2; quoted = 1; continue;
        }
        if (isspace((unsigned char)c) || strchr(";&|<>()'\"\\", c)) break;
        bput(&word, c); j++;
    }
    if (word.n == 0) { free(word.p); return -1; }
    /* re.sub(r"\\(.)", r"\1", word) then remove every quote char. */
    buf_t unesc = {0};
    for (size_t k = 0; k < word.n; k++) {
        if (word.p[k] == '\\' && k + 1 < word.n) { bput(&unesc, word.p[k + 1]); k++; }
        else bput(&unesc, word.p[k]);
    }
    buf_t d = {0};
    for (size_t k = 0; k < unesc.n; k++) if (unesc.p[k] != '\'' && unesc.p[k] != '"') bput(&d, unesc.p[k]);
    char *delim = bdone(&d);
    free(unesc.p); free(word.p);
    *delim_out = delim; *strip_out = strip; *quoted_out = quoted;
    return (long long)j;
}

typedef struct { int quote; long long parens; int arith; } frame_t;
typedef struct { frame_t *v; size_t n, cap; } frames_t;
typedef struct { char *delim; int strip, quoted; } heredoc_t;
typedef struct { heredoc_t *v; size_t n, cap; } heredocs_t;
typedef struct { long long a, b; } span_t;
typedef struct { span_t *v; size_t n, cap; } spans_t;

static void frames_push(frames_t *f, int quote, long long parens, int arith) {
    if (f->n == f->cap) { size_t nc = f->cap ? f->cap * 2 : 8; frame_t *nv = (frame_t *)realloc(f->v, nc * sizeof(frame_t)); if (!nv) return; f->v = nv; f->cap = nc; }
    f->v[f->n].quote = quote; f->v[f->n].parens = parens; f->v[f->n].arith = arith; f->n++;
}
static void heredocs_push(heredocs_t *h, char *delim, int strip, int quoted) {
    if (h->n == h->cap) { size_t nc = h->cap ? h->cap * 2 : 8; heredoc_t *nv = (heredoc_t *)realloc(h->v, nc * sizeof(heredoc_t)); if (!nv) { free(delim); return; } h->v = nv; h->cap = nc; }
    h->v[h->n].delim = delim; h->v[h->n].strip = strip; h->v[h->n].quoted = quoted; h->n++;
}
static void spans_push(spans_t *s, long long a, long long b) {
    if (s->n == s->cap) { size_t nc = s->cap ? s->cap * 2 : 8; span_t *nv = (span_t *)realloc(s->v, nc * sizeof(span_t)); if (!nv) return; s->v = nv; s->cap = nc; }
    s->v[s->n].a = a; s->v[s->n].b = b; s->n++;
}

/* _quoted_heredoc_body_spans -> JSON array of [start, end]. */
static json_t *heredoc_bodies(const char *cmd, long long newline, const json_t *pending) {
    json_t *out = json_array();
    if (!out || !pending || pending->type != JSON_ARRAY) return out;
    long long n = slen(cmd);
    for (size_t hi = 0; hi < json_len(pending); hi++) {
        const json_t *item = json_get(pending, hi);
        if (!item || item->type != JSON_ARRAY || json_len(item) < 3) continue;
        const json_t *d0 = json_get(item, 0);
        const char *delim = (d0 && d0->type == JSON_STRING && d0->str_val) ? d0->str_val : "";
        const json_t *s0 = json_get(item, 1), *q0 = json_get(item, 2);
        int strip = s0 && s0->type == JSON_BOOL && s0->bool_val;
        int quoted = q0 && q0->type == JSON_BOOL && q0->bool_val;
        long long body_start = newline + 1;
        int matched = 0;
        while (newline < n) {
            long long end = find_char(cmd, '\n', (size_t)(newline + 1));
            if (end < 0) end = n;
            buf_t line = {0};
            for (long long p = newline + 1; p < end; p++) bput(&line, cmd[p]);
            (void)bdone(&line);
            newline = end;
            size_t trail = 0;
            while (trail < line.n && line.p[line.n - 1 - trail] == '\\') trail++;
            if (!quoted && end < n && (trail % 2)) {
                if (line.n) line.n--;       /* line = line[:-1] */
                if (line.p) line.p[line.n] = 0;  /* NUL at the NEW length */
                free(line.p);
                continue;
            }
            const char *cmp = line.p ? line.p : "";
            if (strip) while (*cmp == '\t') cmp++;
            if (!strcmp(cmp, delim)) { matched = 1; }
            free(line.p);
            if (matched) break;
        }
        if (quoted) { json_t *pair = json_array(); json_append(pair, json_number((double)body_start)); json_append(pair, json_number((double)newline)); json_append(out, pair); }
        if (newline == n) break;
    }
    return out;
}

/* substitution_depth over the whole command, with explicit frame/heredoc state. */
static int substitution_depth_rec(const char *cmd) {
    frames_t frames = {0}; heredocs_t heredocs = {0}; spans_t skips = {0};
    frames_push(&frames, 0, 0, 0);
    int deepest = 0;
    long long i = 0, n = slen(cmd);
    while (i < n) {
        if (skips.n && i == skips.v[0].a) { long long e = skips.v[0].b; for (size_t k = 1; k < skips.n; k++) skips.v[k - 1] = skips.v[k]; skips.n--; i = e; continue; }
        char ch = cmd[i];
        frame_t *frame = &frames.v[frames.n - 1];
        if (ch == '#' && frame->quote == 0 && (i == 0 || cmd[i - 1] != ')') && is_shell_comment_start(cmd, (size_t)i)) {
            i = find_char(cmd, '\n', (size_t)i);
            if (i < 0) break;
            continue;
        }
        if (i + 2 < n && cmd[i] == '<' && cmd[i + 1] == '<' && cmd[i + 2] == '<') { i += 3; continue; }
        if (ch == '<' && frame->quote == 0 && !frame->arith) {
            char *delim = NULL; int strip = 0, quoted = 0;
            long long e = match_heredoc(cmd, (size_t)i, &delim, &strip, &quoted);
            if (e >= 0) { heredocs_push(&heredocs, delim, strip, quoted); i = e; continue; }
            free(delim);
        }
        if (ch == '\n' && frame->quote == 0 && heredocs.n) {
            json_t *pending = json_array();
            for (size_t k = 0; k < heredocs.n; k++) {
                json_t *t = json_array();
                json_append(t, json_string(heredocs.v[k].delim));
                json_append(t, json_bool(heredocs.v[k].strip));
                json_append(t, json_bool(heredocs.v[k].quoted));
                json_append(pending, t);
            }
            json_t *bodies = heredoc_bodies(cmd, i, pending);
            for (size_t k = 0; k < json_len(bodies); k++) {
                const json_t *b = json_get(bodies, k);
                if (b && b->type == JSON_ARRAY && json_len(b) >= 2)
                    spans_push(&skips, (long long)node_num(json_get(b, 0)), (long long)node_num(json_get(b, 1)));
            }
            json_free(bodies); json_free(pending);
            for (size_t k = 0; k < heredocs.n; k++) free(heredocs.v[k].delim);
            heredocs.n = 0;
        }
        if (ch == '\'' && frame->quote == 0) {
            long long end = find_char(cmd, '\'', (size_t)(i + 1));
            if (end < 0) end = n;
            char *inner = dupn(cmd + i + 1, (size_t)(end - i - 1));
            int sub = substitution_depth_rec(inner ? inner : "");
            free(inner);
            int cand = (int)frames.n - 1 + sub;
            if (cand > deepest) deepest = cand;
            i = end;
        } else if (ch == '\\') {
            i += 1;
        } else if (ch == '$' && i + 1 < n && cmd[i + 1] == '(') {
            int arith = (i + 2 < n && cmd[i + 2] == '(') ? 1 : 0;
            frames_push(&frames, 0, 0, arith);
            if ((int)frames.n - 1 > deepest) deepest = (int)frames.n - 1;
            i += 1;
        } else if (frame->quote == 2) {
            frame->quote = (ch == '"') ? 0 : 2;
        } else if (ch == '"') {
            frame->quote = 2;
        } else if (ch == '(') {
            frame->parens += 1;
        } else if (ch == ')') {
            if (frame->parens) frame->parens -= 1;
            else if (frames.n > 1) frames.n--;
        }
        i += 1;
    }
    for (size_t k = 0; k < heredocs.n; k++) free(heredocs.v[k].delim);
    free(frames.v); free(heredocs.v); free(skips.v);
    return deepest;
}

/* ---- _scan_shell lexical steps ---- */
typedef struct { const char *kind; long long i, j; int has_j; int quote; } step_t;
typedef struct { step_t *v; size_t n, cap; } steps_t;
static void step_push(steps_t *s, const char *kind, long long i, long long j, int has_j, int quote) {
    if (s->n == s->cap) { size_t nc = s->cap ? s->cap * 2 : 32; step_t *nv = (step_t *)realloc(s->v, nc * sizeof(step_t)); if (!nv) return; s->v = nv; s->cap = nc; }
    s->v[s->n].kind = kind; s->v[s->n].i = i; s->v[s->n].j = j; s->v[s->n].has_j = has_j; s->v[s->n].quote = quote; s->n++;
}
static long long backtick_end(const char *cmd, long long start);
static long long dollar_paren_end(const char *cmd, long long start);

static steps_t scan_steps(const char *cmd, long long start, long long n, const char *subst,
                          int brace, int stop_unterminated, int naive_backtick, int comments) {
    steps_t out = {0};
    int quote = 0;
    long long i = start;
    while (i < n) {
        char ch = cmd[i];
        const char *kind = "char";
        long long j = i + 1;
        int has_j = 1;
        if (comments && quote == 0 && is_shell_comment_start(cmd, (size_t)i)) {
            kind = "comment";
            long long p = find_char(cmd, '\n', (size_t)i);
            if (p < 0 || p > n) p = n;   /* text.find('\n', i, n) */
            j = p;
        } else if (quote != 1 && ch == '\\' && i + 1 < n) {
            kind = "esc"; j = i + 2;
        } else if ((quote && ch == (quote == 1 ? '\'' : '"')) || (!quote && (ch == '\'' || ch == '"'))) {
            kind = "quote";
        } else if (quote != 1 && strchr(subst, quote ? 'q' : 'u') &&
                   (ch == '`' || (ch == '$' && i + 1 < n && cmd[i + 1] == '(') ||
                    (brace && !quote && ch == '$' && i + 1 < n && cmd[i + 1] == '{'))) {
            long long close = -1;
            if (ch == '`') {
                if (naive_backtick) { long long p = find_char(cmd, '`', (size_t)(i + 1)); close = p >= 0 ? p + 1 : -1; }
                else close = backtick_end(cmd, i);
            } else if (ch == '$' && cmd[i + 1] == '(') {
                close = dollar_paren_end(cmd, i);
            } else {
                long long p = find_char(cmd, '}', (size_t)(i + 2)); close = p >= 0 ? p + 1 : -1;
            }
            if (close >= 0) { kind = "subst"; j = close; }
            else if (stop_unterminated) { step_push(&out, "subst", i, 0, 0, quote); return out; }
        }
        step_push(&out, kind, i, j, has_j, quote);
        if (!strcmp(kind, "quote")) quote = quote ? 0 : ((ch == '\'') ? 1 : 2);
        i = j;
    }
    return out;
}

static long long backtick_end(const char *cmd, long long start) {
    /* re.compile(r"(?:\\.|[^`\\])*`", re.DOTALL).match(command, start+1) */
    long long n = slen(cmd), j = start + 1;
    while (j < n) {
        char c = cmd[j];
        if (c == '\\') { if (j + 1 >= n) break; j += 2; continue; }
        if (c == '`') break;
        j += 1;
    }
    return (j < n && cmd[j] == '`') ? j + 1 : -1;
}

static long long dollar_paren_end(const char *cmd, long long start) {
    int depth = 1;
    steps_t st = scan_steps(cmd, start + 2, slen(cmd), "", 0, 0, 0, 0);
    for (size_t k = 0; k < st.n; k++) {
        step_t *s = &st.v[k];
        if (!strcmp(s->kind, "char") && s->quote == 0 && s->i < slen(cmd)) {
            if (cmd[s->i] == '$' && s->i + 1 < slen(cmd) && cmd[s->i + 1] == '(') depth++;
            if (cmd[s->i] == ')') depth--;
            if (depth == 0) { long long r = s->i + 1; free(st.v); return r; }
        }
    }
    free(st.v);
    return -1;
}

/* ---- exports ---- */
static json_t *num_or_null(long long v) { return v < 0 ? json_null() : json_number((double)v); }

struct json_t *w16_approvalpa_cd9d4d_substitution_depth(const char *command) {
    return json_number((double)substitution_depth_rec(command ? command : ""));
}

struct json_t *w16_approvalpa_cd9d4d_scan_shell(const struct json_t *opts) {
    const char *text = json_get_str(opts, "text", "");
    long long start = (long long)json_get_num(opts, "start", 0);
    const json_t *ev = json_obj_get(opts, "end");
    long long n = (ev && ev->type == JSON_NUMBER) ? (long long)ev->num_val : slen(text);
    const char *subst = json_get_str(opts, "subst", "");
    int brace = json_get_bool(opts, "brace", false);
    int stop_unterminated = json_get_bool(opts, "stop_unterminated", false);
    int naive_backtick = json_get_bool(opts, "naive_backtick", false);
    int comments = json_get_bool(opts, "comments", false);
    steps_t st = scan_steps(text, start, n, subst ? subst : "", brace, stop_unterminated, naive_backtick, comments);
    json_t *out = json_array();
    for (size_t k = 0; k < st.n; k++) {
        json_t *row = json_array();
        json_append(row, json_string(st.v[k].kind));
        json_append(row, json_number((double)st.v[k].i));
        json_append(row, st.v[k].has_j ? json_number((double)st.v[k].j) : json_null());
        json_append(row, st.v[k].quote ? json_string(st.v[k].quote == 1 ? "'" : "\"") : json_null());
        json_append(out, row);
    }
    free(st.v);
    return out;
}

struct json_t *w16_approvalpa_cd9d4d_dollar_paren_end(const char *command, long long start) {
    return num_or_null(dollar_paren_end(command ? command : "", start));
}
struct json_t *w16_approvalpa_cd9d4d_backtick_end(const char *command, long long start) {
    return num_or_null(backtick_end(command ? command : "", start));
}
struct json_t *w16_approvalpa_cd9d4d_heredoc_bodies(const char *command, long long newline, const struct json_t *pending) {
    return heredoc_bodies(command ? command : "", newline, pending);
}
