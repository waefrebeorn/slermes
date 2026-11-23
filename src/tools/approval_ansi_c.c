/*
 * approval_ansi_c.c — faithful C11 port of upstream Hermes tools/approval_ansi_c.py.
 * PoP: w8_approvalans_fc15b6_expand @ tools/approval_ansi_c.py:expand_ansi_c_quotes
 * PoP: w8_approvalans_fc15b6_span_end @ tools/approval_ansi_c.py:_ansi_c_span_end
 * PoP: w8_approvalans_fc15b6_is_shell_comment_start @ tools/approval_ansi_c.py:_is_shell_comment_start
 * PoP: w8_approvalans_fc15b6_skip_continuations @ tools/approval_ansi_c.py:_skip_continuations
 */
#include "noc_w8_approvalans_fc15b6.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* growable byte buffer */
typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 2 > b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; b->p = realloc(b->p, nc); if (!b->p) return; b->cap = nc; }
    b->p[b->n++] = c; b->p[b->n] = 0;
}
static void badd(buf_t *b, const char *s, size_t len) {
    while (b->n + len + 1 > b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; if (nc < b->n + len + 1) nc = b->n + len + 1; b->p = realloc(b->p, nc); if (!b->p) return; b->cap = nc; }
    memcpy(b->p + b->n, s, len); b->n += len; b->p[b->n] = 0;
}
static void bputs(buf_t *b, const char *s) { badd(b, s, strlen(s)); }
static void bcodepoint(buf_t *b, unsigned cp) {
    if (cp < 0x80) bput(b, (char)cp);
    else if (cp < 0x800) { bput(b, (char)(0xC0 | (cp >> 6))); bput(b, (char)(0x80 | (cp & 0x3F))); }
    else if (cp < 0x10000) { bput(b, (char)(0xE0 | (cp >> 12))); bput(b, (char)(0x80 | ((cp >> 6) & 0x3F))); bput(b, (char)(0x80 | (cp & 0x3F))); }
    else { bput(b, (char)(0xF0 | (cp >> 18))); bput(b, (char)(0x80 | ((cp >> 12) & 0x3F))); bput(b, (char)(0x80 | ((cp >> 6) & 0x3F))); bput(b, (char)(0x80 | (cp & 0x3F))); }
}
static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int is_surrogate(unsigned c) { return c >= 0xD800 && c <= 0xDFFF; }

/* _ESCAPE_RE.sub(_decode_escape, body) then split at first NUL. */
static char *decode_body(const char *body) {
    buf_t lit = {0};
    size_t j = 0, n = strlen(body);
    while (j < n) {
        if (body[j] != '\\' || j + 1 >= n) { bput(&lit, body[j++]); continue; }
        char c = body[j + 1];
        size_t consumed = 2;
        if (c == 'x' || c == 'u' || c == 'U') {
            size_t maxd = (c == 'x') ? 2 : (c == 'u') ? 4 : 8;
            size_t k = j + 2, cnt = 0; unsigned code = 0;
            while (cnt < maxd && k < n && hexval(body[k]) >= 0) { code = code * 16 + (unsigned)hexval(body[k]); k++; cnt++; }
            if (cnt) {
                if (code <= 0x10FFFF && !is_surrogate(code)) bcodepoint(&lit, code);
                else { bput(&lit, '\\'); bput(&lit, c); badd(&lit, body + j + 2, cnt); }
                j = k; continue;
            }
            /* no digits: regex falls through to (.) with named = the letter */
        } else if (c >= '0' && c <= '7') {
            size_t k = j + 1, cnt = 0; unsigned code = 0;
            while (cnt < 3 && k < n && body[k] >= '0' && body[k] <= '7') { code = code * 8 + (unsigned)(body[k] - '0'); k++; cnt++; }
            bcodepoint(&lit, code & 0xFF); j = k; continue;
        } else if (c == 'c' && j + 2 < n) {
            unsigned code = (unsigned char)body[j + 2];
            bcodepoint(&lit, code & 0x1F); j += 3; continue;
        }
        /* named fallback */
        unsigned named = 0;
        switch (c) {
            case 'a': named = 0x07; break; case 'b': named = 0x08; break;
            case 'e': case 'E': named = 0x1B; break; case 'f': named = 0x0C; break;
            case 'n': named = 0x0A; break; case 'r': named = 0x0D; break;
            case 't': named = 0x09; break; case 'v': named = 0x0B; break;
            case '\\': named = 0x5C; break; case '\'': named = 0x27; break;
            case '"': named = 0x22; break; case '?': named = 0x3F; break;
            default: break;
        }
        if (named) bcodepoint(&lit, named);
        else { bput(&lit, '\\'); bput(&lit, c); }
        j += 2;
    }
    /* bash ends the string at a decoded NUL */
    if (lit.p) {
        for (size_t i = 0; i < lit.n; i++) if (lit.p[i] == 0) { lit.n = i; lit.p[i] = 0; break; }
    }
    if (!lit.p) { lit.p = malloc(1); if (lit.p) lit.p[0] = 0; }
    return lit.p ? lit.p : strdup("");
}
static char *single_quoted(const char *body) {
    char *lit = decode_body(body);
    buf_t b = {0};
    bput(&b, '\'');
    for (const char *p = lit; *p; p++) {
        if (*p == '\'') bputs(&b, "'\"'\"'");
        else bput(&b, *p);
    }
    bput(&b, '\'');
    free(lit);
    return b.p ? b.p : strdup("''");
}

/* Scan for the closing quote starting just after the opening quote. */
static long long scan_span(const char *t, long long pos) {
    long long n = (long long)strlen(t);
    while (pos < n) {
        if (t[pos] == '\\') { if (pos + 1 < n) pos += 2; else pos++; }
        else if (t[pos] == '\'') return pos + 1;
        else pos++;
    }
    return -1;
}
long long w8_approvalans_fc15b6_span_end(const char *text, long long start) { return scan_span(text, start + 2); }

int w8_approvalans_fc15b6_is_shell_comment_start(const char *text, long long index) {
    long long n = (long long)strlen(text);
    if (index >= n || index < 0 || text[index] != '#') return 0;
    if (index == 0) return 1;
    char prev = text[index - 1];
    if (!(isspace((unsigned char)prev) || strchr(";&|()<>", prev))) return 0;
    long long j = index - 2;
    while (j >= 0 && text[j] == '\\') j--;
    return ((index - 2 - j) % 2) == 0;
}
long long w8_approvalans_fc15b6_skip_continuations(const char *text, long long i) {
    long long n = (long long)strlen(text);
    while (i + 1 < n && text[i] == '\\' && text[i + 1] == '\n') i += 2;
    return i;
}

typedef struct { char kind; long long depth; } ctx_t;
char *w8_approvalans_fc15b6_expand(const char *command) {
    if (!command) return strdup("");
    if (!strchr(command, '$')) return strdup(command);
    buf_t out = {0};
    ctx_t *st = malloc(64 * sizeof(*st));
    size_t sn = 1, scap = 64;
    st[0].kind = 'u'; st[0].depth = 0;
    long long i = 0, n = (long long)strlen(command);
    while (i < n) {
        char ch = command[i];
        ctx_t *ctx = &st[sn - 1];
        if (i + 1 < n && ch == '\\' && command[i + 1] == '\n') { i += 2; continue; }
        if (ch == '\\') {
            bput(&out, ch);
            if (i + 1 < n) bput(&out, command[i + 1]);
            i += 2; continue;
        }
        long long after = (ch == '$') ? w8_approvalans_fc15b6_skip_continuations(command, i + 1) : i + 1;
        char following = (after < n) ? command[after] : '\0';
        if (ch == '$' && following == '$') { bputs(&out, "$$"); i = after + 1; continue; }
        if (ctx->kind == '"') {
            if (ch == '"') { if (sn > 1) sn--; bput(&out, ch); i++; continue; }
            if ((ch == '$' && following == '(') || ch == '`') {
                if (sn == scap) { scap *= 2; st = realloc(st, scap * sizeof(*st)); }
                st[sn].kind = (ch == '$') ? '(' : '`'; st[sn].depth = 0; sn++;
                bputs(&out, (ch == '$') ? "$(" : "`");
                i = (ch == '$') ? after + 1 : i + 1; continue;
            }
            bput(&out, ch); i++; continue;
        }
        if (ch == '$' && following == '\'') {
            long long end = scan_span(command, after + 1);
            if (end > 0) {
                long long bs = after + 1, be = end - 1;
                size_t blen = (size_t)(be > bs ? be - bs : 0);
                char *body = malloc(blen + 1);
                if (body) { memcpy(body, command + bs, blen); body[blen] = 0; char *q = single_quoted(body); bputs(&out, q); free(q); free(body); }
                i = end; continue;
            }
        }
        if (ch == '$' && following == '"') { i = after; continue; }
        if (ch == '\'') {
            long long e = i + 1;
            while (e < n && command[e] != '\'') e++;
            long long end = (e < n) ? e + 1 : n;
            badd(&out, command + i, (size_t)(end - i));
            i = end; continue;
        }
        if (ch == '#' && w8_approvalans_fc15b6_is_shell_comment_start(command, i)) {
            long long e = i;
            while (e < n && command[e] != '\n') e++;
            badd(&out, command + i, (size_t)(e - i));
            i = e; continue;
        }
        if (ch == '"') { if (sn == scap) { scap *= 2; st = realloc(st, scap * sizeof(*st)); } st[sn].kind = '"'; st[sn].depth = 0; sn++; }
        else if (ctx->kind == '`' && ch == '`') { if (sn > 1) sn--; }
        else if (ctx->kind == '(' && ch == '(') { ctx->depth++; }
        else if (ctx->kind == '(' && ch == ')') { if (ctx->depth) ctx->depth--; else if (sn > 1) sn--; }
        bput(&out, ch); i++;
    }
    free(st);
    if (!out.p) return strdup("");
    return out.p;
}