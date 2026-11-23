/*
 * approval_smart.c — faithful C11 port of the pure shell-comment and verdict
 * helpers in tools/approval_smart.py (gate w7_approvalsma_cffb5c).
 *
 * PoP: w7_approvalsma_cffb5c_apprsmart_next_comment @ tools/approval_smart.py:_next_comment
 * PoP: w7_approvalsma_cffb5c_apprsmart_comment_spans @ tools/approval_smart.py:_comment_spans
 * PoP: w7_approvalsma_cffb5c_apprsmart_strip_line_comment @ tools/approval_smart.py:_strip_line_comment
 * PoP: w7_approvalsma_cffb5c_apprsmart_strip_shell_comments @ tools/approval_smart.py:_strip_shell_comments
 * PoP: w7_approvalsma_cffb5c_apprsmart_has_heredoc @ tools/approval_smart.py:_has_heredoc
 * PoP: w7_approvalsma_cffb5c_apprsmart_verdict @ tools/approval_smart.py:_smart_approve
 *
 * _scan_shell (tools/approval_parser_budget.py) is reimplemented here with
 * the same subst="uq", brace=True semantics used by approval_smart.
 * Fixtures use ASCII so byte offsets == Python character offsets.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w7_approvalsma_cffb5c.h"

/* ---- _scan_shell ------------------------------------------------------- */
enum { K_CHAR = 0, K_ESC = 1, K_QUOTE = 2, K_SUBST = 3 };
enum { Q_NONE = 0, Q_SINGLE = 1, Q_DOUBLE = 2 };
enum { SUB_U = 1, SUB_Q = 2 };

typedef struct { int kind; long i; long j; int quote; } Step;

static int starts_at(const char *t, long i, const char *s) {
    return strncmp(t + i, s, strlen(s)) == 0;
}

/* _scan_backtick_end: regex (?:\\.|[^`\\])*` matched from start+1. */
static long scan_backtick_end(const char *t, long start) {
    long n = (long)strlen(t);
    long j = start + 1;
    while (j < n) {
        char c = t[j];
        if (c == '`') return j + 1;
        if (c == '\\') {
            if (j + 1 < n) { j += 2; continue; }
            break; /* trailing backslash: no closing backtick */
        }
        j++;
    }
    return -1; /* None */
}

static void scan_shell_impl(const char *t, long start, long end, int subst, int brace,
                            Step **out, long *count);

/* _scan_dollar_paren_end: balanced $(...) counting only unquoted chars. */
static long dollar_paren_impl(const char *t, long start) {
    Step *steps = NULL; long n = 0; long result = -1;
    scan_shell_impl(t, start + 2, -1, 0, 0, &steps, &n);
    int depth = 1;
    for (long k = 0; k < n; k++) {
        Step s = steps[k];
        if (s.kind == K_CHAR && s.quote == Q_NONE) {
            if (starts_at(t, s.i, "$(")) depth++;
            if (t[s.i] == ')') depth--;
            if (depth == 0) { result = s.i + 1; break; }
        }
    }
    free(steps);
    return result;
}

static void scan_shell_impl(const char *t, long start, long end, int subst, int brace,
                            Step **out, long *count) {
    long n = end < 0 ? (long)strlen(t) : end;
    int quote = Q_NONE;
    long i = start;
    long cap = 16, cnt = 0;
    Step *arr = (Step *)malloc(cap * sizeof(Step));
    if (!arr) { *out = NULL; *count = 0; return; }
    while (i < n) {
        char ch = t[i];
        int kind = K_CHAR;
        long j = i + 1;
        if (quote != Q_SINGLE && ch == '\\' && i + 1 < n) {
            kind = K_ESC; j = i + 2;
        } else if ((quote == Q_SINGLE && ch == '\'') ||
                   (quote == Q_DOUBLE && ch == '"') ||
                   (quote == Q_NONE && (ch == '\'' || ch == '"'))) {
            kind = K_QUOTE;
        } else {
            int allowed = (quote == Q_NONE) ? (subst & SUB_U)
                        : (quote == Q_DOUBLE ? (subst & SUB_Q) : 0);
            if (quote != Q_SINGLE && allowed &&
                (ch == '`' || starts_at(t, i, "$(") ||
                 (brace && quote == Q_NONE && starts_at(t, i, "${")))) {
                long close = -1;
                if (ch == '`') close = scan_backtick_end(t, i);
                else if (t[i + 1] == '(') close = dollar_paren_impl(t, i);
                else {
                    const char *p = strchr(t + i + 2, '}');
                    close = p ? (long)(p - t) + 1 : -1;
                }
                if (close >= 0) { kind = K_SUBST; j = close; }
            }
        }
        if (cnt == cap) {
            cap *= 2;
            Step *tmp = (Step *)realloc(arr, cap * sizeof(Step));
            if (!tmp) break;
            arr = tmp;
        }
        arr[cnt].kind = kind; arr[cnt].i = i; arr[cnt].j = j; arr[cnt].quote = quote;
        cnt++;
        if (kind == K_QUOTE) quote = (quote == Q_NONE) ? (ch == '\'' ? Q_SINGLE : Q_DOUBLE) : Q_NONE;
        i = j;
    }
    *out = arr; *count = cnt;
}

static void scan_uq(const char *t, long start, long end, Step **out, long *count) {
    scan_shell_impl(t, start, end, SUB_U | SUB_Q, 1, out, count);
}

/* ---- _next_comment ----------------------------------------------------- */
static long next_comment(const char *text, long start) {
    Step *steps = NULL; long n = 0; long result = -1;
    scan_uq(text, start, -1, &steps, &n);
    int word_start = 1, have_padding = 0;
    long padding = 0;
    for (long k = 0; k < n; k++) {
        Step s = steps[k];
        char ch = text[s.i];
        int unquoted = (s.kind == K_CHAR && s.quote == Q_NONE);
        if (unquoted && ch == '#' && word_start) {
            result = have_padding ? padding : s.i;
            break;
        }
        if (unquoted && (ch == ' ' || ch == '\t')) {
            if (!have_padding) { have_padding = 1; padding = s.i; }
        } else {
            have_padding = 0;
        }
        word_start = unquoted && (ch == ' ' || ch == '\t' || ch == '\n' ||
                                  ch == ';' || ch == '&' || ch == '|' ||
                                  ch == '(' || ch == ')' || ch == '<' || ch == '>');
    }
    free(steps);
    return result;
}

/* ---- _comment_spans ---------------------------------------------------- */
typedef struct { long start, end; } Span;

static void comment_spans(const char *text, Span **out, long *count) {
    long len = (long)strlen(text);
    long cap = 8, cnt = 0;
    Span *arr = (Span *)malloc(cap * sizeof(Span));
    if (!arr) { *out = NULL; *count = 0; return; }
    long s = next_comment(text, 0);
    while (s >= 0) {
        const char *nl = strchr(text + s, '\n');
        long e = nl ? (long)(nl - text) : len;
        if (cnt == cap) {
            cap *= 2; Span *tmp = (Span *)realloc(arr, cap * sizeof(Span));
            if (!tmp) break; arr = tmp;
        }
        arr[cnt].start = s; arr[cnt].end = e; cnt++;
        s = (e < len) ? next_comment(text, e) : -1;
    }
    *out = arr; *count = cnt;
}

/* ---- _has_heredoc ------------------------------------------------------ */
static int has_heredoc(const char *text) {
    Span *spans = NULL; long nsp = 0;
    comment_spans(text, &spans, &nsp);
    long len = (long)strlen(text);
    int found = 0;
    long pos = 0;
    for (long k = 0; k <= nsp && !found; k++) {
        long start = (k < nsp) ? spans[k].start : len;
        long end = (k < nsp) ? spans[k].end : len;
        Step *steps = NULL; long n = 0;
        scan_uq(text, pos, start, &steps, &n);
        int after_lt = 0;
        for (long m = 0; m < n; m++) {
            Step s = steps[m];
            int op = (s.kind == K_CHAR && s.quote == Q_NONE);
            if (op && starts_at(text, s.i, "<<") && !starts_at(text, s.i, "<<<") && !after_lt) {
                found = 1; break;
            }
            after_lt = op && (text[s.i] == '<');
        }
        free(steps);
        pos = end;
    }
    free(spans);
    return found;
}

/* ---- public exports ---------------------------------------------------- */
struct json_t *apprsmart_next_comment(const char *text, long start) {
    if (!text) return json_null();
    long r = next_comment(text, start < 0 ? 0 : start);
    return r < 0 ? json_null() : json_number((double)r);
}

struct json_t *apprsmart_comment_spans(const char *text) {
    json_t *arr = json_array();
    if (!text) return arr;
    Span *spans = NULL; long n = 0;
    comment_spans(text, &spans, &n);
    for (long i = 0; i < n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_number((double)spans[i].start));
        json_append(pair, json_number((double)spans[i].end));
        json_append(arr, pair);
    }
    free(spans);
    return arr;
}

struct json_t *apprsmart_strip_line_comment(const char *line) {
    if (!line) return json_string("");
    long s = next_comment(line, 0);
    if (s < 0) return json_string(line);
    char *buf = (char *)malloc((size_t)s + 1);
    if (!buf) return json_string("");
    memcpy(buf, line, (size_t)s); buf[s] = '\0';
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

static const char *const MARKERS[] = {
    "<(", ">(", "$(", "$[", "${", "$'", "$\"", "`",
    "@(", "?(", "*(", "+(", "!(", "=(", "[[", "(("
};

static char *replace_backslash_newline(const char *cmd, int *changed) {
    size_t len = strlen(cmd);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (cmd[i] == '\\' && i + 1 < len && cmd[i + 1] == '\n') {
            i++; *changed = 1;
        } else {
            buf[o++] = cmd[i];
        }
    }
    buf[o] = '\0';
    return buf;
}

struct json_t *apprsmart_strip_shell_comments(const char *command) {
    if (!command) return json_string("");
    int changed = 0;
    char *marker_text = replace_backslash_newline(command, &changed);
    if (!marker_text) return json_string(command);
    for (size_t i = 0; i < sizeof(MARKERS) / sizeof(MARKERS[0]); i++) {
        if (strstr(marker_text, MARKERS[i])) {
            free(marker_text);
            return json_string(command);
        }
    }
    if (changed && strstr(marker_text, "<<")) {
        free(marker_text);
        return json_string(command);
    }
    free(marker_text);

    Span *spans = NULL; long n = 0;
    comment_spans(command, &spans, &n);
    if (n == 0) { free(spans); return json_string(command); }
    if (has_heredoc(command)) { free(spans); return json_string(command); }

    size_t len = strlen(command);
    char *buf = (char *)malloc(len + 1);
    if (!buf) { free(spans); return json_string(command); }
    size_t o = 0;
    long pos = 0; int has_content = 0;
    for (long k = 0; k < n; k++) {
        long s = spans[k].start, e = spans[k].end;
        if (s > pos) { memcpy(buf + o, command + pos, (size_t)(s - pos)); o += (size_t)(s - pos); has_content = 1; }
        pos = e;
        if (pos < (long)len && !has_content) pos++;
    }
    if (pos < (long)len) { memcpy(buf + o, command + pos, (size_t)(len - pos)); o += (size_t)(len - pos); }
    buf[o] = '\0';
    free(spans);
    json_t *r = json_string(buf);
    free(buf);
    return r;
}

struct json_t *apprsmart_has_heredoc(const char *command) {
    if (!command) return json_bool(0);
    return json_bool(has_heredoc(command));
}

struct json_t *apprsmart_verdict(const char *answer) {
    if (!answer) answer = "";
    while (*answer && isspace((unsigned char)*answer)) answer++;
    size_t len = strlen(answer);
    while (len > 0 && isspace((unsigned char)answer[len - 1])) len--;
    if (len == 0) return json_string("escalate");
    char *buf = (char *)malloc(len + 1);
    if (!buf) return json_string("escalate");
    for (size_t i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)answer[i]);
    buf[len] = '\0';
    const char *r = "escalate";
    if (!strcmp(buf, "APPROVE")) r = "approve";
    else if (!strcmp(buf, "DENY")) r = "deny";
    free(buf);
    return json_string(r);
}
