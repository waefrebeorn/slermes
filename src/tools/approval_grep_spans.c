/*
 * approval_grep_spans.c — C home for upstream Hermes tools/approval_grep_spans.py.
 *
 * PoP: w12_approvalgr_415c58_quoted_grep_pattern_spans    @ tools/approval_grep_spans.py:_quoted_grep_pattern_spans
 * PoP: w12_approvalgr_415c58_grep_safe_detection_variant  @ tools/approval_grep_spans.py:_grep_safe_detection_variant
 *
 * Self-contained port: a small quote-aware shell-word lexer (mirroring the
 * scripted oracle seam), option parsing for grep/egrep, and the splice that
 * blanks only wholly single-quoted PCRE pattern operands.  The oracle drives
 * the LIVE functions with the same scripted helper seams.
 */
#include "noc_w12_approvalgr_415c58.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 256
#define MAX_INDEXES 128
#define MAX_SPANS 128

typedef struct {
    char *value;      /* dequoted value */
    char *raw;        /* raw source slice */
    size_t start, end;
    int quoted;       /* wholly single-quoted => inert data */
} tok_t;

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char lower_c(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static void free_tokens(tok_t *t, size_t n) {
    for (size_t i = 0; i < n; ++i) { free(t[i].value); free(t[i].raw); }
}

/* Small lexer: split on whitespace outside quotes; quote chars are dropped
 * from value, kept in raw; a wholly single-quoted raw word is "inert". */
static size_t lex_words(const char *s, size_t from, tok_t *out, size_t cap) {
    size_t n = strlen(s), i = from, count = 0;
    while (i < n && count < cap) {
        while (i < n && is_space(s[i])) i++;
        if (i >= n) break;
        size_t st = i;
        size_t capbuf = n - st + 1;
        char *buf = (char *)malloc(capbuf + 1);
        if (!buf) break;
        size_t bn = 0;
        char quote = 0;
        while (i < n) {
            char ch = s[i];
            if (quote) {
                if (ch == quote) quote = 0;
                else buf[bn++] = ch;
            } else if (ch == '\'' || ch == '"') {
                quote = ch;
            } else if (is_space(ch)) {
                break;
            } else {
                buf[bn++] = ch;
            }
            i++;
        }
        buf[bn] = '\0';
        out[count].value = buf;
        out[count].raw = (char *)malloc(i - st + 1);
        if (out[count].raw) {
            memcpy(out[count].raw, s + st, i - st);
            out[count].raw[i - st] = '\0';
        }
        out[count].start = st;
        out[count].end = i;
        size_t rawlen = i - st;
        out[count].quoted = (rawlen >= 2 && s[st] == '\'' && s[i - 1] == '\'') ? 1 : 0;
        count++;
    }
    return count;
}

static char *basename_of(const char *s) {
    const char *b = s;
    for (const char *p = s; *p; ++p)
        if (*p == '/' || *p == '\\') b = p + 1;
    return strdup(b);
}

static char *deobfuscate(const char *raw) {
    size_t n = strlen(raw);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t k = 0;
    size_t start = 0, end = n;
    if (n >= 2 && ((raw[0] == '\'' && raw[n - 1] == '\'') ||
                   (raw[0] == '"' && raw[n - 1] == '"'))) { start = 1; end = n - 1; }
    for (size_t i = start; i < end; ++i) {
        if (raw[i] == '\\') { if (i + 1 < end) out[k++] = raw[++i]; }
        else out[k++] = raw[i];
    }
    out[k] = '\0';
    return out;
}

static int long_takes_arg(const char *opt) {
    static const char *names[] = {
        "--after-context", "--before-context", "--binary-files", "--context", "--directories",
        "--devices", "--exclude", "--exclude-dir", "--exclude-from", "--include", "--label",
        "--max-count", "--regexp", "--file", NULL
    };
    for (int i = 0; names[i]; ++i) if (!strcmp(opt, names[i])) return 1;
    return 0;
}
static int short_takes_arg(char c) { return strchr("ABCDdefm", c) != NULL; }

typedef struct { size_t s, e; } span_t;

/* returns 1 = malformed (spans cleared) */
static int quoted_grep_spans(const char *command, span_t *spans, size_t *nspans) {
    *nspans = 0;
    if (!command) return 0;
    tok_t words[MAX_TOKENS];
    size_t nw = lex_words(command, 0, words, MAX_TOKENS);
    if (nw == 0) return 0;
    char *cmdname = deobfuscate(words[0].raw);
    char *base = basename_of(cmdname ? cmdname : "");
    int is_grep = 0;
    if (base) {
        char low[16];
        size_t n = strlen(base);
        if (n < sizeof(low)) {
            for (size_t i = 0; i < n; ++i) low[i] = lower_c(base[i]);
            low[n] = '\0';
            is_grep = (!strcmp(low, "grep") || !strcmp(low, "egrep"));
        }
    }
    free(cmdname); free(base);
    if (!is_grep) { free_tokens(words, nw); return 0; }

    tok_t *args = words + 1;
    size_t na = nw - 1;
    size_t idx[MAX_INDEXES]; size_t nidx = 0;
    int pcre = 0, explicit_patterns = 0;
    int operand_index = -1;
    int options_on = 1;
    int malformed = 0;

    for (size_t i = 0; i < na; ++i) {
        const char *token = args[i].value ? args[i].value : "";
        if (options_on && !strcmp(token, "--")) { options_on = 0; }
        else if (options_on && token[0] == '-' && token[1] == '-') {
            const char *eq = strchr(token, '=');
            size_t olen = eq ? (size_t)(eq - token) : strlen(token);
            char *opt = (char *)malloc(olen + 1);
            if (opt) {
                memcpy(opt, token, olen); opt[olen] = '\0';
                if (!strcmp(opt, "--perl-regexp")) pcre = 1;
                if (!strcmp(opt, "--regexp") || !strcmp(opt, "--file")) explicit_patterns = 1;
                int takes_next = long_takes_arg(opt) && !eq;
                if (takes_next && i + 1 >= na) malformed = 1;
                if (!strcmp(opt, "--regexp") && nidx < MAX_INDEXES)
                    idx[nidx++] = takes_next ? i + 1 : i;
                if (!malformed && takes_next) i++;
                free(opt);
            }
        } else if (options_on && token[0] == '-' && token[1]) {
            size_t nchars = strlen(token) - 1;
            const char *chars = token + 1;
            for (size_t j = 0; j < nchars; ++j) {
                char c = chars[j];
                if (c == 'P') pcre = 1;
                if (c == 'e' || c == 'f') explicit_patterns = 1;
                if (short_takes_arg(c)) {
                    int attached = (j + 1 < nchars);
                    if (!attached && i + 1 >= na) malformed = 1;
                    if (c == 'e' && nidx < MAX_INDEXES)
                        idx[nidx++] = attached ? i : i + 1;
                    if (!malformed && !attached) i++;
                    break;
                }
            }
        } else if (operand_index < 0) {
            operand_index = (int)i;
        }
        if (malformed) break;
    }
    if (!malformed && !explicit_patterns) {
        if (operand_index < 0) {
            if (pcre) malformed = 1;
        } else if (nidx < MAX_INDEXES) {
            idx[nidx++] = (size_t)operand_index;
        }
    }
    if (!malformed && pcre) {
        for (size_t k = 0; k < nidx && *nspans < MAX_SPANS; ++k) {
            size_t ix = idx[k];
            if (ix < na && args[ix].quoted) {
                spans[*nspans].s = args[ix].start;
                spans[*nspans].e = args[ix].end;
                (*nspans)++;
            }
        }
    }
    free_tokens(words, nw);
    return malformed;
}

static int cmp_span(const void *a, const void *b) {
    const span_t *x = (const span_t *)a, *y = (const span_t *)b;
    return (x->s > y->s) - (x->s < y->s);
}

static char *splice_spaces(const char *cmd, span_t *spans, size_t n) {
    if (!n) return strdup(cmd ? cmd : "");
    qsort(spans, n, sizeof(span_t), cmp_span);
    size_t len = strlen(cmd);
    size_t cap = len + n * 2 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0, prev = 0;
    for (size_t i = 0; i < n; ++i) {
        size_t s = spans[i].s, e = spans[i].e;
        if (e < s || e > len || s < prev) continue;
        memcpy(out + o, cmd + prev, s - prev); o += s - prev;
        for (size_t j = s; j < e; ++j) out[o++] = ' ';
        prev = e;
    }
    memcpy(out + o, cmd + prev, len - prev); o += len - prev;
    out[o] = '\0';
    return out;
}

json_t *w12_approvalgr_415c58_quoted_grep_pattern_spans(const char *command) {
    span_t spans[MAX_SPANS];
    size_t n = 0;
    int malformed = quoted_grep_spans(command, spans, &n);
    json_t *arr = json_array();
    for (size_t i = 0; i < n; ++i) {
        json_t *pair = json_array();
        json_append(pair, json_number((double)spans[i].s));
        json_append(pair, json_number((double)spans[i].e));
        json_append(arr, pair);
    }
    json_t *out = json_object();
    json_set(out, "spans", arr);
    json_set(out, "malformed", json_bool(malformed));
    return out;
}

json_t *w12_approvalgr_415c58_grep_safe_detection_variant(const char *command) {
    span_t spans[MAX_SPANS];
    size_t n = 0;
    int malformed = quoted_grep_spans(command, spans, &n);
    json_t *out = json_object();
    if (malformed || n == 0) {
        json_set(out, "command", json_string(command ? command : ""));
    } else {
        char *safe = splice_spaces(command, spans, n);
        json_set(out, "command", json_string(safe ? safe : (command ? command : "")));
        free(safe);
    }
    json_set(out, "malformed", json_bool(malformed));
    return out;
}
