/*
 * browser_url_policy.c — C port of upstream Hermes tools/browser_url_policy.py.
 *
 * PoP: w11_browserurl_aac45b_url_literals @ tools/browser_url_policy.py:url_literals
 *
 * The literal scanner reproduces _JS_URL_LITERAL_RE / _URL_OR_SCHEME_RELATIVE_RE,
 * _URL_BODY, _LITERAL_HEAD_RE and the userinfo-tail reconstruction by hand.
 */
#include "noc_w11_browserurl_aac45b.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLIT 32
#define LEN 4096

typedef struct { size_t start, end; } span;

static int body_start_ok(char c) {
    if (isspace((unsigned char)c)) return 0;
    return !strchr("'\"`) ]<>", (int)c);
}
static int body_char_ok(char c) { return body_start_ok(c) && c != '['; }

/* Match _URL_BODY from p; returns end offset (0 = no match). */
static size_t body_match(const char *t, size_t len, size_t p) {
    if (p >= len || !body_start_ok(t[p])) return 0;
    size_t q = p;
    while (q < len && body_char_ok(t[q])) q++;
    if (q < len && t[q] == '[') {
        size_t r = q + 1;
        while (r < len && body_char_ok(t[r])) r++;
        if (r < len && t[r] == ']') {
            q = r + 1;
            while (q < len && body_char_ok(t[q])) q++;
        }
    }
    return q;
}

static int starts_ci(const char *t, size_t len, size_t p, const char *s) {
    size_t n = strlen(s);
    if (p + n > len) return 0;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)t[p + i]) != tolower((unsigned char)s[i])) return 0;
    return 1;
}

static int word_or_sep(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == ':' || c == '/' || c == '\\';
}

static void add_found(char found[MAXLIT][LEN], int *n, const char *s) {
    if (*n < MAXLIT) { snprintf(found[*n], LEN, "%s", s); (*n)++; }
}

/* (?:https?:)?[/\\]+ matched at the start of a literal. */
static size_t head_len(const char *s) {
    size_t n = strlen(s), i = 0;
    if (n >= 5 && !strncasecmp(s, "http:", 5)) i = 5;
    else if (n >= 6 && !strncasecmp(s, "https:", 6)) i = 6;
    size_t start = i;
    while (i < n && (s[i] == '/' || s[i] == '\\')) i++;
    return i > start ? i : 0;
}

struct json_t *w11_browserurl_aac45b_url_literals(const char *text, int protocol_relative) {
    const char *t = text ? text : "";
    size_t len = strlen(t);
    char found[MAXLIT][LEN];
    int nf = 0;
    size_t pos = 0;
    while (pos < len) {
        span m = {0, 0};
        size_t body_start = 0;
        if (starts_ci(t, len, pos, "https://")) { m.start = pos; body_start = pos + 8; }
        else if (starts_ci(t, len, pos, "http://")) { m.start = pos; body_start = pos + 7; }
        if (body_start) {
            size_t e = body_match(t, len, body_start);
            if (e) { m.end = e; }
            else body_start = 0;
        }
        if (!body_start && protocol_relative) {
            if (pos > 0 && word_or_sep(t[pos - 1])) { pos++; continue; }
            size_t j = pos;
            while (j < len && (t[j] == '/' || t[j] == '\\')) j++;
            size_t run = j - pos;
            if (run >= 2 && j < len && t[j] != '/' && t[j] != '\\' && !isspace((unsigned char)t[j])) {
                size_t e = body_match(t, len, j);
                if (e) { m.start = pos; m.end = e; }
            }
        }
        if (!m.end) { pos++; continue; }
        size_t ml = m.end - m.start;
        if (ml >= LEN) ml = LEN - 1;
        char literal[LEN];
        memcpy(literal, t + m.start, ml); literal[ml] = 0;
        add_found(found, &nf, literal);
        size_t hl = head_len(literal);
        int authority_fixed = 0;
        for (size_t k = hl; k < strlen(literal); k++)
            if (strchr("/?#\\", (int)literal[k])) { authority_fixed = 1; break; }
        if (!authority_fixed) {
            /* _USERINFO_TAIL_RE.match(text, m.end()): [^\s'"`/?#\\]*@ */
            size_t q = m.end, body_from = 0;
            while (q < len && !isspace((unsigned char)t[q]) && !strchr("'\"`/?#\\", (int)t[q])) {
                if (t[q] == '@') body_from = q + 1;  /* greedy * backtracks to the last @ */
                q++;
            }
            if (body_from) {
                q = body_from;
                size_t he = body_match(t, len, q);
                if (he) {
                    char rebuilt[LEN];
                    snprintf(rebuilt, sizeof(rebuilt), "%.*s%.*s", (int)hl, literal,
                             (int)(he - q), t + q);
                    add_found(found, &nf, rebuilt);
                }
            }
        }
        pos = m.end;
    }
    json_t *out = json_array();
    for (int i = 0; i < nf; i++) {
        char *s = found[i];
        size_t n = strlen(s);
        while (n && strchr(".,;", (int)s[n - 1])) s[--n] = 0;
        json_append(out, json_string(s));
    }
    return out;
}
