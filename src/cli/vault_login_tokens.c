/*
 * vault_login_tokens.c - login-form tokenisation helpers.
 *
 * Split from port_vault_login_classifier.c (soul.md: self-contained
 * modules). Unicode-case folding, tokenisation and token-set membership
 * tests. No file-scope state.
 */
#include "vault_login_tokens.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ── small helpers ───────────────────────────────────────────────── */ char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}


/* Latin-1 Supplement NFKD base letters (the ones form labels actually use). */
static const char *fold_codepoint(unsigned cp)
{
    switch (cp) {
    case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5:
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return "a";
    case 0xC6: case 0xE6: return "ae";
    case 0xC7: case 0xE7: return "c";
    case 0xC8: case 0xC9: case 0xCA: case 0xCB:
    case 0xE8: case 0xE9: case 0xEA: case 0xEB: return "e";
    case 0xCC: case 0xCD: case 0xCE: case 0xCF:
    case 0xEC: case 0xED: case 0xEE: case 0xEF: return "i";
    case 0xD0: case 0xF0: return "d";
    case 0xD1: case 0xF1: return "n";
    case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8:
    case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return "o";
    case 0xD9: case 0xDA: case 0xDB: case 0xDC:
    case 0xF9: case 0xFA: case 0xFB: case 0xFC: return "u";
    case 0xDD: case 0xFD: case 0xFF: return "y";
    case 0xDE: case 0xFE: return "th";
    case 0xDF: return "ss";
    default: return NULL;
    }
}


/* PoP: vl_normalize_text @ agent/vault_login_classifier.py:_normalize_text */
char *vl_normalize_text(const char *value)
{
    if (!value) return dup_str("");
    size_t cap = strlen(value) * 4 + 8;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;

    const unsigned char *p = (const unsigned char *)value;
    while (*p) {
        unsigned cp = *p;
        int bytes = 1;
        if (cp >= 0xF0) { cp = 0; bytes = 4; }
        else if (cp >= 0xE0) {
            cp = (unsigned)(p[0] & 0x0F);
            for (int k = 1; k < 3; k++) cp = (cp << 6) | (p[k] & 0x3F);
            bytes = 3;
        } else if (cp >= 0xC0) {
            cp = (unsigned)(p[0] & 0x1F);
            cp = (cp << 6) | (p[1] & 0x3F);
            bytes = 2;
        }

        if (cp < 0x80) {
            unsigned char c = (unsigned char)cp;
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                out[o++] = (char)c;
            } else if (c >= 'A' && c <= 'Z') {
                out[o++] = (char)tolower(c);
            } else {
                if (o && out[o - 1] != ' ') out[o++] = ' ';
            }
        } else {
            const char *fold = fold_codepoint(cp);
            if (fold) {
                for (const char *q = fold; *q; q++) out[o++] = *q;
            } else if (o && out[o - 1] != ' ') {
                out[o++] = ' ';
            }
        }
        p += bytes;
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = '\0';
    return out;
}
 int tokenize(const char *normalized, char **tokens, int max)
{
    int n = 0;
    const char *p = normalized;
    while (*p && n < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ' ') p++;
        size_t len = (size_t)(p - start);
        char *tok = (char *)malloc(len + 1);
        if (!tok) break;
        memcpy(tok, start, len);
        tok[len] = '\0';
        tokens[n++] = tok;
    }
    return n;
}
 int has_tok(char **t, int n, const char *w)
{
    for (int i = 0; i < n; i++) if (strcmp(t[i], w) == 0) return 1;
    return 0;
}
 int any_tok(char **t, int n, const char *const *set, size_t ns)
{
    for (size_t k = 0; k < ns; k++) if (has_tok(t, n, set[k])) return 1;
    return 0;
}
 int pair_at(char **t, int n, int i, const char *a, const char *b)
{
    return i + 1 < n && strcmp(t[i], a) == 0 && strcmp(t[i + 1], b) == 0;
}


static int triple_at(char **t, int n, int i, const char *a, const char *b,
                     const char *c)
{
    return i + 2 < n && strcmp(t[i], a) == 0 && strcmp(t[i + 1], b) == 0 &&
           strcmp(t[i + 2], c) == 0;
}
 int has_pair(char **t, int n, const char *a, const char *b)
{
    for (int i = 0; i + 1 < n; i++) if (pair_at(t, n, i, a, b)) return 1;
    return 0;
}
 int has_triple(char **t, int n, const char *a, const char *b, const char *c)
{
    for (int i = 0; i + 2 < n; i++) if (triple_at(t, n, i, a, b, c)) return 1;
    return 0;
}
 void vl_free_tokens(char **t, int n)
{
    for (int i = 0; i < n; i++) free(t[i]);
}
