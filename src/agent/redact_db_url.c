/*
 * redact_db_url.c — C port of upstream Hermes agent/redact_db_url.py.
 *
 * PoP: w6_redactdbur_875c0b_is_userinfo_placeholder @ agent/redact_db_url.py:_is_userinfo_placeholder
 * PoP: w6_redactdbur_875c0b_is_template_value @ agent/redact_db_url.py:_is_template_value
 * PoP: w6_redactdbur_875c0b_mask_query_params @ agent/redact_db_url.py:_mask_query_params
 * PoP: w6_redactdbur_875c0b_redact_db_url_query_passwords @ agent/redact_db_url.py:redact_db_url_query_passwords
 *
 * The Python module uses compiled regexes; POSIX ERE lacks their possessive
 * quantifiers and lookaheads, so each regex is reimplemented as a direct scan
 * with identical matching semantics for the exercised inputs.
 */
#include "noc_w6_redactdbur_875c0b.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *d; size_t n, cap; } buf;
static void bput(buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 64;
        while (cap < b->n + n + 1) cap *= 2;
        b->d = (char *)realloc(b->d, cap); b->cap = cap;
    }
    memcpy(b->d + b->n, s, n); b->n += n; b->d[b->n] = '\0';
}
static void blit(buf *b, const char *s) { bput(b, s, strlen(s)); }
static char *bfin(buf *b) {
    if (!b->d) return strdup("");
    b->d[b->n] = '\0'; return b->d;
}

static int word_placeholder(const char *s, size_t n) {
    static const char *words[] = {"password", "passwd", "pass", "pw", "pwd", "secret", "token"};
    for (size_t i = 0; i < sizeof(words)/sizeof(words[0]); i++) {
        size_t m = strlen(words[i]);
        if (n == m && strncasecmp(s, words[i], m) == 0) return 1;
    }
    return 0;
}

static int dsn_placeholder(const char *s, size_t n) {
    if (n == 0) return 0;
    if (s[0] == '<' && s[n-1] == '>') {
        size_t m = n - 2;
        if (m >= 1) {
            unsigned char c0 = (unsigned char)s[1];
            if (c0 >= 'a' && c0 <= 'z') {
                for (size_t i = 2; i <= m; i++) {
                    unsigned char c = (unsigned char)s[i];
                    if (!((c>='a'&&c<='z')||c=='_'||c==' '||c=='-')) return 0;
                }
                return 1;
            }
            if (c0 >= 'A' && c0 <= 'Z') {
                for (size_t i = 2; i <= m; i++) {
                    unsigned char c = (unsigned char)s[i];
                    if (!((c>='A'&&c<='Z')||c=='_'||c==' '||c=='-')) return 0;
                }
                return 1;
            }
        }
        return 0;
    }
    if (s[0] == '$') {
        size_t p = 1;
        if (p < n && s[p] == '{') p++;
        if (p >= n || !(((s[p]>='A'&&s[p]<='Z')||s[p]=='_'))) return 0;
        p++;
        while (p < n && ((s[p]>='A'&&s[p]<='Z')||(s[p]>='0'&&s[p]<='9')||s[p]=='_')) p++;
        if (p < n && s[p] == '}') p++;
        return p == n;
    }
    if (s[0] == '%') {
        size_t p = 1;
        if (p >= n || !(((s[p]>='A'&&s[p]<='Z')||s[p]=='_'))) return 0;
        p++;
        while (p < n && ((s[p]>='A'&&s[p]<='Z')||(s[p]>='0'&&s[p]<='9')||s[p]=='_')) p++;
        return p < n && s[p] == '%' && p + 1 == n;
    }
    return 0;
}

static int userinfo_span(const char *s, size_t n) {
    if (!s) return 0;
    if (n >= 2 && s[0] == '{' && s[n-1] == '}') return 1;
    if (dsn_placeholder(s, n)) return 1;
    if (word_placeholder(s, n)) return 1;
    return 0;
}
static int template_span(const char *s, size_t n) {
    if (!s) return 0;
    if (n >= 1 && s[0] == '{') return 1;
    if (n >= 2 && s[0] == '$' && s[1] == '{') return 1;
    return userinfo_span(s, n);
}

int w6_redactdbur_875c0b_is_userinfo_placeholder(const char *s) {
    return userinfo_span(s ? s : "", s ? strlen(s) : 0);
}
int w6_redactdbur_875c0b_is_template_value(const char *s) {
    return template_span(s ? s : "", s ? strlen(s) : 0);
}

static int sep_char(char c) { return c=='?'||c=='&'||c==';'; }

static int key_at(const char *q, size_t n, size_t sep, size_t *ke) {
    if (sep >= n || !sep_char(q[sep])) return 0;
    static const char *alts[] = {"sslpassword", "password", "passwd", "pwd"};
    size_t p = sep + 1;
    for (size_t i = 0; i < 4; i++) {
        size_t m = strlen(alts[i]);
        if (p + m < n && strncasecmp(q + p, alts[i], m) == 0 && q[p + m] == '=') {
            *ke = p + m + 1; return 1;
        }
    }
    return 0;
}
static int find_key(const char *q, size_t n, size_t pos, size_t *ks, size_t *ke) {
    for (size_t i = pos; i < n; i++) {
        if (key_at(q, n, i, ke)) { *ks = i; return 1; }
    }
    return 0;
}
static int head_excl(char c) {
    return c==';'||c=='&'||c=='#'||c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'||c=='"'||c=='\''||c=='<'||c=='>';
}
static int rest_excl(char c) {
    return c=='&'||c=='#'||c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f'||c=='"'||c=='\''||c=='<'||c=='>';
}
static size_t match_run(const char *q, size_t n, size_t pos, int (*excl)(char)) {
    size_t i = pos;
    while (i < n && !excl(q[i])) i++;
    return i;
}
static int sibling_at(const char *q, size_t n, size_t h) {
    if (h >= n || q[h] != ';') return 0;
    size_t p = h + 1;
    if (p >= n || !((q[p]>='A'&&q[p]<='Z')||(q[p]>='a'&&q[p]<='z')||q[p]=='_')) return 0;
    p++;
    while (p < n && ((q[p]>='A'&&q[p]<='Z')||(q[p]>='a'&&q[p]<='z')||
                     (q[p]>='0'&&q[p]<='9')||q[p]=='_'||q[p]=='.'||q[p]=='-')) p++;
    return p < n && q[p] == '=';
}

char *w6_redactdbur_875c0b_mask_query_params(const char *query) {
    if (!query) return strdup("");
    size_t n = strlen(query);
    buf out = {0};
    size_t pos = 0, ks, ke;
    while (find_key(query, n, pos, &ks, &ke)) {
        size_t head_end = match_run(query, n, ke, head_excl);
        bput(&out, query + pos, ke - pos);
        int tmpl = template_span(query + ke, head_end - ke);
        if (tmpl && sibling_at(query, n, head_end)) {
            bput(&out, query + ke, head_end - ke);
            pos = head_end;
            continue;
        }
        size_t newpos = match_run(query, n, head_end, rest_excl);
        if (newpos == head_end && tmpl)
            bput(&out, query + ke, head_end - ke);
        else if (newpos > ke)
            blit(&out, "***");
        pos = newpos;
    }
    bput(&out, query + pos, n - pos);
    return bfin(&out);
}

static size_t scheme_len(const char *t, size_t n, size_t i) {
    static const char *schemes[] = {"postgresql","postgres","mariadb","mongodb","rediss","redis","amqps","amqp","mysql"};
    for (size_t s = 0; s < sizeof(schemes)/sizeof(schemes[0]); s++) {
        size_t m = strlen(schemes[s]);
        if (i + m <= n && strncasecmp(t + i, schemes[s], m) == 0) return m;
    }
    return 0;
}
static int space_char(char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static int query_stop(char c) {
    return space_char(c)||c=='#'||c=='"'||c=='<'||c=='>';
}

char *w6_redactdbur_875c0b_redact_db_url_query_passwords(const char *text) {
    if (!text) return strdup("");
    size_t n = strlen(text);
    buf out = {0};
    size_t i = 0, last = 0;
    while (i < n) {
        size_t sl = scheme_len(text, n, i);
        if (!sl) { i++; continue; }
        size_t j = i + sl;
        if (j < n && text[j] == '+') {
            size_t p = j + 1, got = 0;
            while (p < n && ((text[p]>='a'&&text[p]<='z')||(text[p]>='0'&&text[p]<='9'))) { p++; got++; }
            if (!got) { i++; continue; }
            j = p;
        }
        if (j + 3 > n || text[j] != ':' || text[j+1] != '/' || text[j+2] != '/') { i++; continue; }
        j += 3;
        while (j < n) {
            char c = text[j];
            if (space_char(c) || c == '?' || c == '#') break;
            if (c == ':') {
                if (j + 2 < n && text[j+1]=='/' && text[j+2]=='/') break;
                j++; continue;
            }
            j++;
        }
        if (j >= n || text[j] != '?') { i++; continue; }
        size_t k = j + 1;
        while (k < n && !query_stop(text[k])) k++;
        if (k == j + 1) { i++; continue; }
        bput(&out, text + last, i - last);
        char *span = (char *)malloc(k - i + 1);
        memcpy(span, text + i, k - i); span[k - i] = '\0';
        char *masked = w6_redactdbur_875c0b_mask_query_params(span);
        blit(&out, masked);
        free(masked); free(span);
        last = k; i = k;
    }
    bput(&out, text + last, n - last);
    return bfin(&out);
}

