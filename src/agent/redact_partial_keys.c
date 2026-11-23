/*
 * redact_partial_keys.c — C home for upstream Hermes agent/redact_partial_keys.py.
 *
 * Faithful line-walking C11 port of the base64-body mask next to a lone armor
 * line.  The three Python regexes are hand-implemented exactly:
 *   ARMOR  re.search(r"-----(BEGIN|END)[A-Z ]*PRIVATE KEY-----")
 *   BODY   re.fullmatch(r"[ \t]*(?![0-9A-Fa-f]+[ \t]*$)[A-Za-z0-9+/=]{16,}[ \t]*")
 *   TRAIL  re.fullmatch(r"[ \t]*(?:[A-Za-z0-9+/]{1,15}={0,2}|==?)[ \t]*")
 *
 * PoP: w11_redactpart_5d0d92_redact @ agent/redact_partial_keys.py:_redact_partial_private_keys
 */
#include "noc_w11_redactpart_5d0d92.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

static int is_b64(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}
static int is_b64_nopad(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/';
}
static int is_hex(int c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/* re.search(ARMOR, line): leftmost match; +1 = BEGIN (forward), -1 = END. */
static int armor_step(const char *line) {
    size_t n = strlen(line);
    for (size_t i = 0; i + 5 <= n; i++) {
        if (strncmp(line + i, "-----", 5)) continue;
        const char *p = line + i + 5;
        const char *kw;
        if (!strncmp(p, "BEGIN", 5)) { kw = "BEGIN"; p += 5; }
        else if (!strncmp(p, "END", 3)) { kw = "END"; p += 3; }
        else continue;
        const char *q = p;
        while (*q && ((*q >= 'A' && *q <= 'Z') || *q == ' ')) q++;
        for (const char *r = q;; r--) {           /* greedy [A-Z ]* with backtracking */
            if (strlen(r) >= 16 && !strncmp(r, "PRIVATE KEY", 11) &&
                !strncmp(r + 11, "-----", 5))
                return kw[0] == 'B' ? 1 : -1;
            if (r == p) break;
        }
    }
    return 0;
}

/* re.fullmatch(BODY, line) on line.rstrip("\r"). */
static int body_match(const char *line) {
    size_t n = strlen(line);
    while (n > 0 && line[n - 1] == '\r') n--;
    const char *a = line, *b = line + n;
    while (a < b && (*a == ' ' || *a == '\t')) a++;
    while (b > a && (b[-1] == ' ' || b[-1] == '\t')) b--;
    if ((size_t)(b - a) < 16) return 0;
    int all_hex = 1;
    for (const char *p = a; p < b; p++) {
        if (!is_b64((unsigned char)*p)) return 0;
        if (!is_hex((unsigned char)*p)) all_hex = 0;
    }
    return all_hex ? 0 : 1;   /* negative lookahead rejects pure-hex digests */
}

/* re.fullmatch(TRAIL, line) on line.rstrip("\r"). */
static int trailer_match(const char *line) {
    size_t n = strlen(line);
    while (n > 0 && line[n - 1] == '\r') n--;
    const char *a = line, *b = line + n;
    while (a < b && (*a == ' ' || *a == '\t')) a++;
    while (b > a && (b[-1] == ' ' || b[-1] == '\t')) b--;
    size_t len = (size_t)(b - a);
    if (!len) return 0;
    int all_eq = 1;
    for (const char *p = a; p < b; p++) if (*p != '=') { all_eq = 0; break; }
    if (all_eq && len <= 2) return 1;              /* ==? */
    size_t i = 0;
    while (i < len && is_b64_nopad((unsigned char)a[i])) i++;
    if (i < 1 || i > 15) return 0;
    size_t eq = 0;
    while (i + eq < len && a[i + eq] == '=') eq++;
    return (eq <= 2 && i + eq == len) ? 1 : 0;
}

/* Python str.rstrip() then endswith("="). */
static int rstrip_ends_eq(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    return n > 0 && s[n - 1] == '=';
}

/* PoP: w11_redactpart_5d0d92_redact @ agent/redact_partial_keys.py:_redact_partial_private_keys */
json_t *w11_redactpart_5d0d92_redact(json_t *a) {
    const char *text = json_get_str(a, "text", "");
    if (!text) text = "";

    size_t nlines = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') nlines++;
    char **lines = (char **)calloc(nlines, sizeof(*lines));
    unsigned char *drop = (unsigned char *)calloc(nlines, 1);
    if (!lines || !drop) { free(lines); free(drop); return json_string(text); }

    size_t li = 0;
    const char *start = text;
    for (const char *p = text;; p++) {
        if (*p == '\n' || *p == '\0') {
            size_t len = (size_t)(p - start);
            char *line = (char *)malloc(len + 1);
            if (!line) { for (size_t k = 0; k < li; k++) free(lines[k]); free(lines); free(drop); return json_string(text); }
            memcpy(line, start, len); line[len] = '\0';
            lines[li++] = line;
            if (*p == '\0') break;
            start = p + 1;
        }
    }

    for (size_t i = 0; i < nlines; i++) {
        int step = armor_step(lines[i]);
        if (!step) continue;
        long long j = (long long)i + step;
        if (step < 0 && j > 0 && (size_t)j < nlines &&
            trailer_match(lines[j]) && body_match(lines[j - 1])) {
            drop[j] = 1;
            j--;
        }
        while (j >= 0 && (size_t)j < nlines && !drop[j] && body_match(lines[j])) {
            drop[j] = 1;
            j += step;
        }
        if (step > 0 && (long long)i + 1 < j && (size_t)j < nlines &&
            trailer_match(lines[j]) && rstrip_ends_eq(lines[j])) {
            drop[j] = 1;
        }
        if (j != (long long)i + step) {
            free(lines[i]);
            lines[i] = dup_str("[REDACTED PRIVATE KEY]");
        }
    }

    size_t cap = strlen(text) + 32;
    char *out = (char *)malloc(cap);
    if (!out) { for (size_t k = 0; k < nlines; k++) free(lines[k]); free(lines); free(drop); return json_string(text); }
    size_t olen = 0;
    int first = 1;
    for (size_t k = 0; k < nlines; k++) {
        if (drop[k]) continue;
        if (!first && olen + 1 < cap) out[olen++] = '\n';
        first = 0;
        size_t l = strlen(lines[k]);
        if (olen + l >= cap) break;
        memcpy(out + olen, lines[k], l);
        olen += l;
    }
    out[olen] = '\0';

    for (size_t k = 0; k < nlines; k++) free(lines[k]);
    free(lines);
    free(drop);
    json_t *result = json_string(out);
    free(out);
    return result;
}
