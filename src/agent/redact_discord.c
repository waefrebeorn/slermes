/*
 * redact_discord.c — C port of upstream Hermes agent/redact_discord.py.
 *
 * PoP: w7_redactdisco_982d70_is_discord_token @ agent/redact_discord.py:_is_discord_token
 * PoP: w7_redactdisco_982d70_mfa_left_ok @ agent/redact_discord.py:_mfa_left_ok
 * PoP: w7_redactdisco_982d70_redact_tokens @ agent/redact_discord.py:_redact_discord_tokens
 *
 * The lookahead-based regexes are reimplemented as overlapping position scans;
 * the ID form is accepted only when its first segment base64-decodes to a
 * 17-20 digit ASCII snowflake, exactly as in the live module.
 */
#include "hermes_json.h"
#include "noc_w7_redactdisco_982d70.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *d; size_t n, cap; } sb;
static void sb_addn(sb *b, const char *s, size_t n) {
    if (!n) return;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->n + n + 1) cap *= 2;
        b->d = (char *)realloc(b->d, cap); b->cap = cap;
    }
    memcpy(b->d + b->n, s, n); b->n += n; b->d[b->n] = '\0';
}
static void sb_add(sb *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static char *sb_take(sb *b) {
    if (!b->d) return (char *)calloc(1,1);
    b->d[b->n] = '\0'; char *r = b->d; b->d = NULL; b->n = b->cap = 0; return r;
}

static int token_char(unsigned char c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';
}
static int seg_char(unsigned char c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';
}
static int hex_char(unsigned char c) {
    return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');
}

/* base64.urlsafe_b64decode(head + padding); decoded must be 17-20 ASCII digits. */
static int is_discord_token(const char *candidate) {
    if (!candidate) return 0;
    const char *dot = strchr(candidate, '.');
    size_t hn = dot ? (size_t)(dot - candidate) : strlen(candidate);
    if (hn == 0) return 0;
    unsigned char vals[64];
    size_t nv = 0;
    for (size_t i = 0; i < hn; i++) {
        unsigned char c = (unsigned char)candidate[i];
        int v;
        if (c>='A'&&c<='Z') v = c-'A';
        else if (c>='a'&&c<='z') v = c-'a'+26;
        else if (c>='0'&&c<='9') v = c-'0'+52;
        else if (c=='-') v = 62;
        else if (c=='_') v = 63;
        else continue; /* validate=False discards non-alphabet chars */
        if (nv < 64) vals[nv++] = (unsigned char)v;
    }
    size_t padded = (nv + 3) & ~(size_t)3;
    if (nv == 0 || padded > 64) return 0;
    unsigned char dec[48]; size_t dn = 0;
    size_t padding = padded - nv;
    for (size_t i = 0; i < padded; i += 4) {
        unsigned long acc = 0;
        int have = 0;
        for (size_t j = 0; j < 4; j++) {
            int v = (i+j < nv) ? vals[i+j] : 0;
            acc = (acc << 6) | (unsigned long)v;
            have += 6;
        }
        size_t bytes = have / 8;
        if (bytes >= 1 && dn < sizeof(dec)) dec[dn++] = (unsigned char)((acc >> 16) & 0xFF);
        if (bytes >= 2 && dn < sizeof(dec)) dec[dn++] = (unsigned char)((acc >> 8) & 0xFF);
        if (bytes >= 3 && dn < sizeof(dec)) dec[dn++] = (unsigned char)(acc & 0xFF);
    }
    if (padding && dn >= padding) dn -= padding;
    if (dn < 17 || dn > 20) return 0;
    for (size_t i = 0; i < dn; i++) if (dec[i] < '0' || dec[i] > '9') return 0;
    return 1;
}

/* Escape spelling whose last char is a letter/digit glued to what follows. */
static int escape_tail(const char *text, size_t start) {
    size_t lo = start > 12 ? start - 12 : 0;
    if (start >= 2 && text[start-2]=='\\' && strchr("nrtbf", text[start-1])) return 1;
    if (start >= 4 && text[start-4]=='\\' && text[start-3]=='x' && hex_char((unsigned char)text[start-2]) && hex_char((unsigned char)text[start-1])) return 1;
    if (start >= 3 && text[start-3]=='%' && hex_char((unsigned char)text[start-2]) && hex_char((unsigned char)text[start-1])) return 1;
    if (start >= 6 && text[start-6]=='\\' && text[start-5]=='u' &&
        hex_char((unsigned char)text[start-4]) && hex_char((unsigned char)text[start-3]) &&
        hex_char((unsigned char)text[start-2]) && hex_char((unsigned char)text[start-1])) return 1;
    /* (ESC | \x1b | \033 | \e)[0-9;]*m */
    if (start >= 2 && text[start-1]=='m') {
        size_t k = start - 1;
        while (k > lo && (text[k-1]>='0'&&text[k-1]<='9'||text[k-1]==';')) k--;
        if (k > lo && text[k-1]=='[') {
            size_t e = k - 1;
            if (e > lo && (unsigned char)text[e-1] == 0x1B) return 1;
            if (e >= lo+4 && !strncmp(text+e-4, "\\x1b", 4)) return 1;
            if (e >= lo+4 && !strncmp(text+e-4, "\\033", 4)) return 1;
            if (e >= lo+2 && !strncmp(text+e-2, "\\e", 2)) return 1;
        }
    }
    (void)lo;
    return 0;
}
static int mfa_left_ok(const char *text, size_t start) {
    if (start == 0 || !token_char((unsigned char)text[start-1])) return 1;
    return escape_tail(text, start);
}

/* TAIL: next char is not [A-Za-z0-9_-] and not '.' followed by such. */
static int tail_ok(const char *s, size_t n, size_t p) {
    if (p >= n) return 1;
    unsigned char c = (unsigned char)s[p];
    if (seg_char(c)) return 0;
    if (c=='.' && p+1<n && seg_char((unsigned char)s[p+1])) return 0;
    return 1;
}
typedef struct { size_t start, len; int mfa; } Match;
/* Zero-width lookahead positions, checked left-to-right (overlapping). */
static size_t find_matches(const char *s, Match *out, size_t cap) {
    size_t n = strlen(s), k = 0;
    for (size_t p = 0; p <= n && k < cap; p++) {
        /* mfa.<84 token chars> */
        if (p + 4 + 84 <= n && !strncmp(s+p, "mfa.", 4)) {
            size_t e = p + 4;
            size_t j = 0;
            while (j < 84 && e < n && seg_char((unsigned char)s[e])) { e++; j++; }
            if (j == 84 && tail_ok(s, n, e)) { out[k].start=p; out[k].len=e-p; out[k].mfa=1; k++; continue; }
        }
        /* [MNO]<22-27>.<6-7>.<27+> */
        if (p < n && (s[p]=='M'||s[p]=='N'||s[p]=='O')) {
            size_t e = p + 1;
            size_t h = 0;
            while (e < n && seg_char((unsigned char)s[e])) { e++; h++; }
            if (h >= 22 && h <= 27 && e < n && s[e]=='.') {
                size_t a = e + 1, m = 0;
                while (a < n && seg_char((unsigned char)s[a])) { a++; m++; }
                if (m >= 6 && m <= 7 && a < n && s[a]=='.') {
                    size_t b = a + 1, t = 0;
                    while (b < n && seg_char((unsigned char)s[b])) { b++; t++; }
                    if (t >= 27 && tail_ok(s, n, b)) { out[k].start=p; out[k].len=b-p; out[k].mfa=0; k++; }
                }
            }
        }
    }
    return k;
}

struct json_t *w7_redactdisco_982d70_is_discord_token(const char *candidate) {
    return json_bool(is_discord_token(candidate) ? true : false);
}
struct json_t *w7_redactdisco_982d70_mfa_left_ok(const char *text, int start) {
    if (!text || start < 0 || (size_t)start > strlen(text)) return json_bool(false);
    return json_bool(mfa_left_ok(text, (size_t)start) ? true : false);
}
struct json_t *w7_redactdisco_982d70_redact_tokens(const char *text, const char *mask) {
    if (!text) return json_string("");
    const char *mk = mask ? mask : "***";
    Match arr[512];
    size_t n = strlen(text), cnt = find_matches(text, arr, 512);
    sb out = {0};
    size_t pos = 0; int found = 0;
    for (size_t i = 0; i < cnt; i++) {
        Match *m = &arr[i];
        size_t start = m->start;
        if (start < pos) continue;
        int ok = m->mfa ? mfa_left_ok(text, start)
                        : is_discord_token(text + start);
        if (!ok) continue;
        sb_addn(&out, text + pos, start - pos);
        sb_add(&out, mk);
        pos = start + m->len; found = 1;
    }
    if (!found) return json_string(text);
    sb_addn(&out, text + pos, n - pos);
    char *r = sb_take(&out); json_t *j = json_string(r); free(r); return j;
}
