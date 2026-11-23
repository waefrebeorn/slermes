/*
 * redact_connstr.c — C port of upstream Hermes agent/redact_connstr.py.
 *
 * PoP: w7_redactconns_217bf3_closes_wrapper @ agent/redact_connstr.py:_closes_wrapper
 * PoP: w7_redactconns_217bf3_kv_value_end @ agent/redact_connstr.py:_kv_value_end
 * PoP: w7_redactconns_217bf3_redact_line @ agent/redact_connstr.py:_redact_kv_connection_line
 * PoP: w7_redactconns_217bf3_redact_strings @ agent/redact_connstr.py:_redact_kv_connection_strings
 *
 * Python compiled regexes (fixed-width lookbehind, IGNORECASE, non-greedy
 * value extents) are reimplemented as direct scans with identical semantics.
 */
#include "hermes_json.h"
#include "noc_w7_redactconns_217bf3.h"
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
static void sb_free(sb *b) { free(b->d); b->d = NULL; b->n = b->cap = 0; }
static char *str_dup_n(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1); memcpy(r, s, n); r[n] = '\0'; return r;
}

static int is_re_space(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static int is_hspace(unsigned char c) { return c==' '||c=='\t'; }
static int is_quote(unsigned char c) { return c=='"'||c=='\''; }
/* (?<![^\s;"']) — the preceding char is whitespace/;/quote, or start. */
static int boundary_ok(const char *s, size_t p) {
    if (p == 0) return 1;
    unsigned char c = (unsigned char)s[p-1];
    return is_re_space(c) || c==';' || is_quote(c);
}
static int ci_at(const char *s, size_t p, const char *word) {
    size_t m = strlen(word);
    for (size_t i = 0; i < m; i++)
        if (p+i == 0 || tolower((unsigned char)s[p+i]) != tolower((unsigned char)word[i])) return 0;
    return 1;
}
static int wrapper_followers(unsigned char c) {
    return c==' '||c=='\t'||c=='\r'||c=='\n'||c==','||c==')'||c==']'||c=='}'||c=='>'||c=='/'||c=='.'||c=='+'||c=='&'||c=='|';
}
static int closes_wrapper(const char *line, size_t n, size_t j) {
    return j + 1 >= n || wrapper_followers((unsigned char)line[j+1]);
}

/* Pattern aliases: literal chars, '~' = \s+, '%' = \s*. Then [ \t]*=. */
static const char *const COMPANION_KEYS[] = {
    "host","server","data~source","address","addr","network~address","user%id","uid",
    "user","username","database","dbname","hostaddr","initial~catalog","port"
};
static const char *const SEMICOLON_KEYS[] = {
    "host","server","data~source","address","addr","network~address","user%id","uid",
    "user","username","database","dbname","hostaddr","initial~catalog","port",
    "password","pwd","driver","encrypt","pooling","integrated~security","trusted_connection",
    "trustservercertificate","sslmode","ssl~mode","timeout","connect~timeout","connection~timeout",
    "application~name","search~path"
};
static int match_alias(const char *s, size_t n, size_t p, const char *pat, size_t *end_out) {
    size_t q = p;
    for (const char *a = pat; *a; a++) {
        if (*a == '~') {
            if (q >= n || !is_re_space((unsigned char)s[q])) return 0;
            while (q < n && is_re_space((unsigned char)s[q])) q++;
        } else if (*a == '%') {
            while (q < n && is_re_space((unsigned char)s[q])) q++;
        } else {
            if (q >= n || tolower((unsigned char)s[q]) != tolower((unsigned char)*a)) return 0;
            q++;
        }
    }
    while (q < n && is_hspace((unsigned char)s[q])) q++;
    if (q >= n || s[q] != '=') return 0;
    *end_out = q + 1;
    return 1;
}
static int match_any_key(const char *s, size_t n, size_t p, const char *const *keys, size_t nk) {
    size_t end;
    for (size_t i = 0; i < nk; i++) if (match_alias(s, n, p, keys[i], &end)) return 1;
    return 0;
}
static int gate_search(const char *s) {
    size_t n = strlen(s);
    for (size_t p = 0; p < n; p++) {
        static const char *const words[] = {"password","pwd"};
        for (size_t w = 0; w < 2; w++) {
            size_t m = strlen(words[w]);
            if (p + m <= n && ci_at(s, p, words[w])) {
                size_t q = p + m;
                while (q < n && is_re_space((unsigned char)s[q])) q++;
                if (q < n && s[q] == '=') return 1;
            }
        }
    }
    return 0;
}
static int companion_search(const char *s) {
    size_t n = strlen(s);
    for (size_t p = 0; p < n; p++)
        if (boundary_ok(s, p) && match_any_key(s, n, p, COMPANION_KEYS, sizeof(COMPANION_KEYS)/sizeof(COMPANION_KEYS[0]))) return 1;
    return 0;
}
static int semicolon_search(const char *s) {
    size_t n = strlen(s);
    for (size_t p = 0; p < n; p++) {
        if (s[p] != ';') continue;
        size_t q = p + 1;
        while (q < n && is_hspace((unsigned char)s[q])) q++;
        if (match_any_key(s, n, q, SEMICOLON_KEYS, sizeof(SEMICOLON_KEYS)/sizeof(SEMICOLON_KEYS[0]))) return 1;
    }
    return 0;
}

/* A password key match with the fixed-width boundary, [ \t]* around '='. */
typedef struct { size_t start, end, eq; } PWMatch;
static int pw_match_at(const char *s, size_t n, size_t p, PWMatch *out) {
    if (!boundary_ok(s, p)) return 0;
    static const char *const words[] = {"password","pwd"};
    for (size_t w = 0; w < 2; w++) {
        size_t m = strlen(words[w]);
        if (p + m > n || !ci_at(s, p, words[w])) continue;
        size_t q = p + m;
        while (q < n && is_hspace((unsigned char)s[q])) q++;
        if (q >= n || s[q] != '=') continue;
        size_t eq = q; q++;
        while (q < n && is_hspace((unsigned char)s[q])) q++;
        out->start = p; out->eq = eq; out->end = q;
        return 1;
    }
    return 0;
}
static size_t pw_find_all(const char *s, PWMatch *arr, size_t cap) {
    size_t n = strlen(s), k = 0, p = 0;
    while (p < n && k < cap) {
        PWMatch m;
        if (pw_match_at(s, n, p, &m)) { arr[k++] = m; p = m.end; }
        else p++;
    }
    return k;
}
static int pw_search_range(const char *s, size_t lo, size_t hi) {
    PWMatch arr[256];
    size_t n = strlen(s), k = pw_find_all(s, arr, 256);
    for (size_t i = 0; i < k; i++) if (arr[i].start >= lo && arr[i].end <= hi) return 1;
    return 0;
}

static size_t kv_value_end(const char *line, size_t i, int semicolon) {
    size_t n = strlen(line);
    if (i >= n) return i;
    char c = line[i];
    if (semicolon) {
        if (c == '{') {
            size_t j = i + 1;
            while (j < n) {
                if (line[j] == '}') {
                    if (j + 1 < n && line[j+1] == '}') { j += 2; continue; }
                    return j + 1;
                }
                j++;
            }
            return n;
        }
        if (is_quote((unsigned char)c)) {
            size_t j = i + 1;
            while (j < n) {
                if (line[j] == c) {
                    if (j + 1 < n && line[j+1] == c) { j += 2; continue; }
                    return j + 1;
                }
                j++;
            }
            return n;
        }
        size_t j = i;
        while (j < n && line[j] != ';' && line[j] != '\r' && line[j] != '\n' &&
               !(is_quote((unsigned char)line[j]) && closes_wrapper(line, n, j))) j++;
        while (j > i && is_hspace((unsigned char)line[j-1])) j--;
        return j;
    }
    if (is_quote((unsigned char)c)) {
        size_t j = i + 1;
        while (j < n) {
            if (line[j] == '\\') { j += 2; continue; }
            if (line[j] == c) return j + 1;
            j++;
        }
        return n;
    }
    size_t j = i;
    while (j < n) {
        if (line[j] == '\\') { j += 2; continue; }
        if (is_re_space((unsigned char)line[j]) || (is_quote((unsigned char)line[j]) && closes_wrapper(line, n, j))) break;
        j++;
    }
    return j < n ? j : n;
}

/* _FORMAT_SLOT_RE.fullmatch: { [ws] [ident [^;{}@$&#!]* (!rsa(?=[:}]))?] } */
static int format_slot(const char *s) {
    size_t n = strlen(s), p = 0;
    if (n == 0 || s[p] != '{') return 0;
    p++;
    while (p < n && is_re_space((unsigned char)s[p])) p++;
    if (p < n && ((s[p]>='A'&&s[p]<='Z')||(s[p]>='a'&&s[p]<='z')||(s[p]>='0'&&s[p]<='9')||s[p]=='_')) {
        p++;
        while (p < n) {
            unsigned char c = (unsigned char)s[p];
            if (c==';'||c=='{'||c=='}'||c=='@'||c=='$'||c=='&'||c=='#') break;
            if (c=='!') {
                if (p+1 < n && (s[p+1]=='r'||s[p+1]=='s'||s[p+1]=='a')) {
                    size_t k = p + 2;
                    while (k < n && is_re_space((unsigned char)s[k])) k++;
                    if (k < n && (s[k]==':'||s[k]=='}')) { p += 2; continue; }
                }
                break;
            }
            p++;
        }
    }
    return p < n && s[p]=='}' && p+1==n;
}
static char *strip_quotes(const char *s, size_t n) {
    size_t i = 0, j = n;
    while (i < j && is_quote((unsigned char)s[i])) i++;
    while (j > i && is_quote((unsigned char)s[j-1])) j--;
    return str_dup_n(s + i, j - i);
}

static char *redact_line(const char *line, int code_file) {
    if (!line) return (char *)calloc(1,1);
    if (!gate_search(line) || !companion_search(line)) return strdup(line);
    size_t n = strlen(line);
    int semicolon = semicolon_search(line);
    PWMatch arr[512];
    size_t cnt = pw_find_all(line, arr, 512);
    sb out = {0};
    size_t pos = 0, scanned = 0;
    int found = 0;
    for (size_t i = 0; i < cnt; i++) {
        PWMatch *m = &arr[i];
        if (m->start < scanned) continue;
        size_t vstart = m->end;
        size_t vend = kv_value_end(line, vstart, semicolon);
        if (vend <= vstart) continue;
        size_t vlen = vend - vstart;
        int last_after_eq = m->end > 0 && is_hspace((unsigned char)line[m->end - 1]);
        int before_eq_space = m->eq > 0 && is_hspace((unsigned char)line[m->eq - 1]);
        if (!vlen || (!semicolon && last_after_eq && !before_eq_space)) continue;
        scanned = vend;
        char *bare = strip_quotes(line + vstart, vlen);
        size_t blen = strlen(bare);
        int starts_tmpl = (blen >= 1 && bare[0]=='{') || (blen >= 2 && bare[0]=='$' && bare[1]=='{');
        int tmpl = code_file && starts_tmpl && blen >= 1 && bare[blen-1]=='}' &&
                   (!semicolon || (blen>=2 && bare[0]=='$' && bare[1]=='{') || format_slot(bare)) &&
                   !pw_search_range(line, vstart, vend);
        free(bare);
        if (tmpl) continue;
        const char *masked = "***";
        char quoted_mask[8];
        char value_first = line[vstart];
        if (value_first=='{' && semicolon) masked = "{***}";
        else if (is_quote((unsigned char)value_first) && vlen > 1 && line[vend-1] == value_first) {
            quoted_mask[0]=value_first; quoted_mask[1]='*'; quoted_mask[2]='*'; quoted_mask[3]='*'; quoted_mask[4]=value_first; quoted_mask[5]='\0';
            masked = quoted_mask;
        }
        sb_addn(&out, line + pos, vstart - pos);
        sb_add(&out, masked);
        pos = vend; found = 1;
    }
    if (!found) { sb_free(&out); return strdup(line); }
    sb_addn(&out, line + pos, n - pos);
    return sb_take(&out);
}

struct json_t *w7_redactconns_217bf3_closes_wrapper(const char *line, int j) {
    if (!line || j < 0) return json_bool(false);
    return json_bool(closes_wrapper(line, strlen(line), (size_t)j) ? true : false);
}
struct json_t *w7_redactconns_217bf3_kv_value_end(const char *line, int i, int semicolon) {
    if (!line || i < 0) return json_number(0);
    return json_number((double)kv_value_end(line, (size_t)i, semicolon));
}
struct json_t *w7_redactconns_217bf3_redact_line(const char *line, int code_file) {
    char *r = redact_line(line ? line : "", code_file);
    json_t *j = json_string(r); free(r); return j;
}
struct json_t *w7_redactconns_217bf3_redact_strings(const char *text, int code_file) {
    if (!text) return json_string("");
    size_t n = strlen(text), pos = 0;
    sb out = {0};
    while (pos < n) {
        size_t e = pos;
        while (e < n && text[e] != '\n' && text[e] != '\r') e++;
        if (e < n) {
            if (text[e]=='\r' && e+1<n && text[e+1]=='\n') e++;
            e++;
        }
        char *line = str_dup_n(text + pos, e - pos);
        char *r = redact_line(line, code_file);
        sb_add(&out, r);
        free(r); free(line);
        pos = e;
    }
    char *r = sb_take(&out); json_t *j = json_string(r); free(r); return j;
}
