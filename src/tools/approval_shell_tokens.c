/*
 * approval_shell_tokens.c — C home for upstream Hermes tools/approval_shell_tokens.py.
 *
 * PoP: w15_approvalsh_f4acbf_backtick_end_from @ tools/approval_shell_tokens.py:_backtick_end_from
 * PoP: w15_approvalsh_f4acbf_shell_tokens_with_spans @ tools/approval_shell_tokens.py:_shell_tokens_with_spans
 *
 * Faithful port of the quote/escape state machine in
 * tools.approval_parser_budget._scan_shell(segment, start) with its default
 * arguments (subst="", brace=False, comments=False), which is the only variant
 * the live module calls. Tokens carry source spans; no shell expansion occurs.
 */
#include "noc_w15_approvalsh_f4acbf.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

long long w15_approvalsh_f4acbf_backtick_end_from(const char *segment, long long i) {
    if (!segment) return -1;
    size_t n = strlen(segment);
    if (i < 0 || (size_t)i >= n) return -1;
    long long j = -1;
    for (size_t k = (size_t)i + 1; k < n; ++k) {
        if (segment[k] == '`') { j = (long long)k; break; }
    }
    /* A backtick preceded by a backslash is escaped: skip to the next one. */
    while (j >= 0 && j > 0 && segment[j - 1] == '\\') {
        long long prev = j;
        j = -1;
        for (size_t k = (size_t)prev + 1; k < n; ++k) {
            if (segment[k] == '`') { j = (long long)k; break; }
        }
    }
    return j;
}

/* growable char buffer for the current token value */
typedef struct { char *s; size_t len, cap; } cbuf;
static void cb_put(cbuf *b, char ch) {
    if (!b->s) {
        b->cap = 32;
        b->s = (char *)malloc(b->cap);
        if (!b->s) return;
        b->s[0] = '\0';
    }
    if (b->len + 2 > b->cap) {
        size_t cap = b->cap * 2;
        while (b->len + 2 > cap) cap *= 2;
        char *p = (char *)realloc(b->s, cap);
        if (!p) return;
        b->s = p; b->cap = cap;
    }
    b->s[b->len++] = ch;
    b->s[b->len] = '\0';
}
static void cb_reset(cbuf *b) {
    b->len = 0;
    if (b->s) b->s[0] = '\0';
}

static int inert_raw(const char *seg, long long start, long long end) {
    size_t n = (size_t)(end - start);
    if (n == 0 || seg[end - 1] != '\'') return 0;      /* raw.endswith("'") */
    if (seg[start] == '\'') return 1;                  /* raw.startswith("'") */
    /* "='" in raw */
    for (size_t i = 0; i + 2 < n; ++i) {
        if (seg[start + i] == '=' && seg[start + i + 1] == '\'') return 1;
    }
    return 0;
}

struct json_t *w15_approvalsh_f4acbf_shell_tokens_with_spans(const char *segment, long long start) {
    if (!segment) return NULL;
    size_t n = strlen(segment);
    long long i = start;
    if (i < 0) i = 0;
    int quote = 0;          /* 0, '\'' or '"' */
    int depth = 0;          /* $(...) opened AFTER start */
    int in_backtick = 0;
    long long end_at = (long long)n;
    long long token_start = -1;
    cbuf value = {0};
    json_t *tokens = json_array();

    while (i < (long long)n) {
        char ch = segment[i];
        int kind;           /* 0=char, 1=esc, 2=quote */
        long long j = i + 1;
        if (quote != '\'' && ch == '\\' && i + 1 < (long long)n) { kind = 1; j = i + 2; }
        else if ((quote && ch == quote) || (!quote && (ch == '\'' || ch == '"'))) { kind = 2; }
        else { kind = 0; }

        if (kind == 0 && !quote) {
            if (isspace((unsigned char)ch) && ch != '\n') {
                if (token_start >= 0) {
                    json_t *tok = json_array();
                    json_append(tok, json_string(value.s ? value.s : ""));
                    json_append(tok, json_number((double)token_start));
                    json_append(tok, json_number((double)i));
                    json_append(tok, json_bool(inert_raw(segment, token_start, i)));
                    json_append(tokens, tok);
                    cb_reset(&value);
                    token_start = -1;
                }
                i = j;
                continue;
            }
            if (ch == '$' && i + 1 < (long long)n && segment[i + 1] == '(') {
                depth++;
            } else if (ch == '`') {
                if (in_backtick) in_backtick = 0;
                else if (depth == 0 && w15_approvalsh_f4acbf_backtick_end_from(segment, i) == -1) {
                    end_at = i;
                    break;
                } else in_backtick = 1;
            } else if (ch == ')') {
                if (depth == 0) { end_at = i; break; }
                depth--;
            } else if (ch == ';' || ch == '|' || ch == '&' || ch == '\n') {
                end_at = i;
                break;
            }
        }
        if (token_start < 0) token_start = i;
        if (kind == 2) {
            quote = quote ? 0 : ch;
        } else if (kind == 1) {
            cb_put(&value, segment[i + 1]);
        } else if (ch == '\\' && !quote) {
            free(value.s);
            json_free(tokens);
            return NULL;             /* dangling backslash */
        } else {
            cb_put(&value, ch);
        }
        i = j;
    }
    if (quote) {
        free(value.s);
        json_free(tokens);
        return NULL;                 /* unterminated quote */
    }
    if (token_start >= 0) {
        json_t *tok = json_array();
        json_append(tok, json_string(value.s ? value.s : ""));
        json_append(tok, json_number((double)token_start));
        json_append(tok, json_number((double)end_at));
        json_append(tok, json_bool(inert_raw(segment, token_start, end_at)));
        json_append(tokens, tok);
    }
    free(value.s);
    return tokens;
}
