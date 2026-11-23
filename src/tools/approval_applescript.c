/*
 * approval_applescript.c — C port of upstream Hermes tools/approval_applescript.py.
 *
 * PoP: w14_approvalap_72b623_mentions_osascript @ tools/approval_applescript.py:_mentions_osascript
 * PoP: w14_approvalap_72b623_strip_layout @ tools/approval_applescript.py:_strip_layout
 * PoP: w14_approvalap_72b623_heredoc_body @ tools/approval_applescript.py:_heredoc_body
 *
 * The self-contained lexical helpers of the AppleScript/JXA classifier. The
 * full shell-word classifier depends on tools.approval_detection and is not
 * reproduced here; these three helpers do real, faithful work on their own.
 */
#include "noc_w14_approvalap_72b623.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ao_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *ao_b(int v) { return json_bool(v ? 1 : 0); }
static int ao_word(unsigned char c) { return isalnum(c) || c == '_'; }

/* -- _mentions_osascript -------------------------------------------------- */
/* Clean text: drop '"' , '\'' and '\\' (re.sub(r"[\"'\\]", "", text)). */
static char *ao_clean(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '"' || c == '\'' || c == '\\') continue;
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}
static int ao_isosa(const char *s, size_t i, size_t n, size_t *end) {
    static const char *PFX = "osa";
    static const char *A = "script";
    static const char *B = "compile";
    if (i + 3 > n) return 0;
    for (int k = 0; k < 3; k++) if (tolower((unsigned char)s[i+k]) != PFX[k]) return 0;
    size_t j = i + 3;
    const char *tail = NULL;
    if (j + 6 <= n) {
        int ok = 1;
        for (int k = 0; k < 6; k++) if (tolower((unsigned char)s[j+k]) != A[k]) { ok = 0; break; }
        if (ok) { tail = A; }
    }
    if (!tail && j + 7 <= n) {
        int ok = 1;
        for (int k = 0; k < 7; k++) if (tolower((unsigned char)s[j+k]) != B[k]) { ok = 0; break; }
        if (ok) { tail = B; }
    }
    if (!tail) return 0;
    *end = j + strlen(tail);
    return 1;
}
static int ao_mentions(const char *s) {
    char *c = ao_clean(s);
    if (!c) return 0;
    size_t n = strlen(c);
    int found = 0;
    for (size_t i = 0; i < n && !found; i++) {
        if (i > 0) {
            unsigned char p = (unsigned char)c[i-1];
            if (ao_word(p) || p == '.' || p == '-') continue;
        }
        size_t end = 0;
        if (!ao_isosa(c, i, n, &end)) continue;
        /* negative lookahead (?![\w-]|\.(?!exe\b)\w) */
        if (end < n) {
            unsigned char e = (unsigned char)c[end];
            if (ao_word(e) || e == '-') continue;
            if (e == '.') {
                size_t k = end + 1;
                if (k < n && ao_word((unsigned char)c[k])) {
                    int isexe = (k + 3 <= n &&
                        tolower((unsigned char)c[k])=='e' && tolower((unsigned char)c[k+1])=='x' &&
                        tolower((unsigned char)c[k+2])=='e' &&
                        (k+3 == n || !ao_word((unsigned char)c[k+3])));
                    if (!isexe) continue;
                }
            }
        }
        found = 1;
    }
    free(c);
    return found;
}

/* -- _strip_layout -------------------------------------------------------- */
/* Buffer builder */
typedef struct { char *p; size_t n, cap; } Buf;
static void ao_append(Buf *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        size_t nc = (b->cap ? b->cap * 2 : 64);
        while (nc < b->n + len + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}
/* Find leftmost layout token (regex _LAYOUT_RE). kind: 0=str 1=bar 2=blockopen 3=line 4=cont */
static int ao_find_layout(const char *s, size_t from, size_t n,
                          size_t *ts, size_t *te, int *kind) {
    for (size_t i = from; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        int k = -1; size_t e = i;
        if (c == '"') {
            k = 0; size_t j = i + 1;
            while (j < n) {
                if (s[j] == '\\') { j += 2; if (j > n) j = n; }
                else if (s[j] == '"') { j++; break; }
                else j++;
            }
            e = j;
        } else if (c == '|') {
            k = 1; size_t j = i + 1;
            while (j < n && s[j] != '|') j++;
            if (j < n) j++;
            e = j;
        } else if (c == '(' && i + 1 < n && s[i+1] == '*') { k = 2; e = i + 2; }
        else if (c == '-' && i + 1 < n && s[i+1] == '-') {
            k = 3; size_t j = i + 2; while (j < n && s[j] != '\n') j++; e = j;
        } else if (c == '#') {
            k = 3; size_t j = i + 1; while (j < n && s[j] != '\n') j++; e = j;
        } else if (c == 0xC2 && i + 1 < n && (unsigned char)s[i+1] == 0xAC) { k = 4; e = i + 2; }
        if (k >= 0) { *ts = i; *te = e; *kind = k; return 1; }
    }
    return 0;
}
static int ao_find_block(const char *s, size_t from, size_t n,
                         size_t *bs, size_t *be, int *is_open) {
    for (size_t i = from; i + 1 < n; i++) {
        if (s[i]=='(' && s[i+1]=='*') { *bs=i; *be=i+2; *is_open=1; return 1; }
        if (s[i]=='*' && s[i+1]==')') { *bs=i; *be=i+2; *is_open=0; return 1; }
    }
    return 0;
}
static char *ao_strip_layout(const char *s) {
    size_t n = strlen(s);
    Buf b = {0};
    size_t i = 0;
    size_t ts, te;
    int kind;
    while (ao_find_layout(s, i, n, &ts, &te, &kind)) {
        ao_append(&b, s + i, ts - i);
        if (kind == 0 || kind == 1) {
            ao_append(&b, s + ts, te - ts);
            i = te;
            continue;
        }
        ao_append(&b, " ", 1);
        size_t j = te;
        int depth = (kind == 2) ? 1 : 0;
        if (depth) {
            size_t bs, be; int op;
            while (ao_find_block(s, j, n, &bs, &be, &op)) {
                depth += op ? 1 : -1;
                j = be;
                if (!depth) break;
            }
            if (depth) {
                if (!b.p) { b.p = (char *)malloc(1); if (b.p) b.p[0]='\0'; }
                return b.p ? b.p : (char *)"";
            }
        }
        i = j;
    }
    ao_append(&b, s + i, n - i);
    if (!b.p) { b.p = (char *)malloc(1); if (b.p) b.p[0]='\0'; }
    return b.p ? b.p : (char *)"";
}

/* -- _heredoc_body -------------------------------------------------------- */
json_t *w14_approvalap_72b623_heredoc_body(const struct json_t *text, const struct json_t *after,
        const struct json_t *delim, const struct json_t *strip_tabs) {
    const char *t = ao_jstr(text);
    if (!t) t = "";
    const char *d = ao_jstr(delim);
    if (!d) d = "";
    size_t n = strlen(t);
    size_t af = 0;
    if (after && after->type == JSON_NUMBER && after->num_val >= 0) af = (size_t)after->num_val;
    if (af > n) af = n;
    size_t nl = af;
    while (nl < n && t[nl] != '\n') nl++;
    json_t *out = json_object();
    if (nl >= n) {
        json_set(out, "body", json_string(""));
        json_set(out, "start", json_number((double)n));
        json_set(out, "end", json_number((double)n));
        return out;
    }
    size_t body_start = nl + 1;
    int strip = strip_tabs && strip_tabs->type == JSON_BOOL && strip_tabs->bool_val;
    size_t dl = strlen(d);
    size_t es = n, ee = n;
    int found = 0;
    size_t line = body_start;
    while (line <= n) {
        size_t p = line;
        if (strip) while (p < n && t[p] == '\t') p++;
        int ok = 1;
        if (p + dl > n) ok = 0;
        else for (size_t k = 0; k < dl; k++) if (t[p+k] != d[k]) { ok = 0; break; }
        if (ok && (p + dl == n || t[p+dl] == '\n')) {
            es = line; ee = p + dl; found = 1; break;
        }
        size_t q = line;
        while (q < n && t[q] != '\n') q++;
        if (q >= n) break;
        line = q + 1;
    }
    size_t body_end = found ? es : n;
    char *body = (char *)malloc(body_end - body_start + 1);
    if (body) {
        memcpy(body, t + body_start, body_end - body_start);
        body[body_end - body_start] = '\0';
        json_set(out, "body", json_string(body));
        free(body);
    } else json_set(out, "body", json_string(""));
    json_set(out, "start", json_number((double)body_start));
    json_set(out, "end", json_number((double)(found ? ee : n)));
    return out;
}

json_t *w14_approvalap_72b623_mentions_osascript(const struct json_t *text) {
    return ao_b(ao_mentions(ao_jstr(text) ? ao_jstr(text) : ""));
}
json_t *w14_approvalap_72b623_strip_layout(const struct json_t *program) {
    char *r = ao_strip_layout(ao_jstr(program) ? ao_jstr(program) : "");
    json_t *out = json_string(r);
    if (r && r[0] != '\0') free(r);
    return out;
}
