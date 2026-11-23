/*
 * file_operations_common.c — real C11 port of upstream
 * tools/file_operations_common.py.
 *
 * PoP: fileopscommon_count_conflict_blocks       @ tools/file_operations_common.py:count_conflict_blocks
 * PoP: fileopscommon_strip_terminal_fence_leaks  @ tools/file_operations_common.py:_strip_terminal_fence_leaks
 * PoP: fileopscommon_filename_match_key          @ tools/file_operations_common.py:filename_match_key
 * PoP: fileopscommon_detect_line_ending          @ tools/file_operations_common.py:_detect_line_ending
 * PoP: fileopscommon_normalize_line_endings      @ tools/file_operations_common.py:_normalize_line_endings
 * PoP: fileopscommon_strip_bom                   @ tools/file_operations_common.py:_strip_bom
 * PoP: fileopscommon_has_bom                     @ tools/file_operations_common.py:_has_bom
 * PoP: fileopscommon_coerce_int                  @ tools/file_operations_common.py:_coerce_int
 * PoP: fileopscommon_normalize_read_pagination   @ tools/file_operations_common.py:normalize_read_pagination
 * PoP: fileopscommon_normalize_search_pagination @ tools/file_operations_common.py:normalize_search_pagination
 * PoP: fileopscommon_lint_result_to_dict         @ tools/file_operations_common.py:LintResult.to_dict
 *
 * Only the pure text helpers and the LintResult emission shape are ported; the
 * dataclasses that wrap I/O results are out of scope for a pure harness.
 */
#include "noc_fileopscommon.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BOM0 ((unsigned char)0xEF)
#define BOM1 ((unsigned char)0xBB)
#define BOM2 ((unsigned char)0xBF)

static char *foc_dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return (char *)calloc(1, 1);
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static char *foc_dup(const char *s) { return foc_dup_n(s ? s : "", s ? strlen(s) : 0); }

/*
 * PoP: fileopscommon_count_conflict_blocks @ tools/file_operations_common.py:count_conflict_blocks
 */
int fileopscommon_count_conflict_blocks(const char *formatted_content) {
    const char *p = formatted_content ? formatted_content : "";
    int opens = 0, closes = 0;
    const char *line = p;
    while (1) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        size_t i = 0;
        while (i < len && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' ||
                           line[i] == '\v' || line[i] == '\f')) i++;
        {
            size_t ds = i;
            while (i < len && isdigit((unsigned char)line[i])) i++;
            if (i > ds && i < len && line[i] == '|') {
                const char *rest = line + i + 1;
                size_t rem = len - i - 1;
                if (rem >= 8 && !strncmp(rest, "<<<<<<< ", 8)) opens++;
                if (rem >= 8 && !strncmp(rest, ">>>>>>> ", 8)) closes++;
            }
        }
        if (!end) break;
        line = end + 1;
    }
    return opens ? (opens < closes ? opens : closes) : 0;
}

/* Remove OSC sequences ESC ] ... BEL / ESC-backslash. */
static char *foc_strip_osc(const char *s) {
    size_t n = strlen(s), o = 0, i = 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return foc_dup("");
    while (i < n) {
        if ((unsigned char)s[i] == 0x1B && i + 1 < n && s[i + 1] == ']') {
            size_t j = i + 2;
            while (j < n && s[j] != 0x07 && s[j] != 0x1B) j++;
            if (j < n && s[j] == 0x07) { i = j + 1; continue; }
            if (j + 1 < n && s[j] == 0x1B && s[j + 1] == '\\') { i = j + 2; continue; }
        }
        out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}

/* Remove __HERMES_FENCE_<alnum>__ markers with optional quote/BEL wrappers. */
static char *foc_strip_fence_markers(const char *s) {
    static const char MARK[] = "__HERMES_FENCE_";
    size_t n = strlen(s), o = 0, i = 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return foc_dup("");
    while (i < n) {
        const char *hit = strstr(s + i, MARK);
        if (!hit) { size_t l = strlen(s + i); memcpy(out + o, s + i, l); o += l; break; }
        {
            size_t st = (size_t)(hit - s);
            if (st >= 2 && s[st - 1] == 0x07 && s[st - 2] == '\'') st -= 2;
            else if (st >= 1 && (s[st - 1] == 0x07 || s[st - 1] == '\'')) st -= 1;
            if (st > o) { size_t l = st - o; memcpy(out + o, s + o, l); o += l; }
            {
                size_t k = (size_t)(hit - s) + sizeof(MARK) - 1;
                size_t a = k;
                while (k < n && isalnum((unsigned char)s[k])) k++;
                if (k >= a && k + 1 < n && s[k] == '_' && s[k + 1] == '_') {
                    k += 2;
                    if (k < n && s[k] == 0x07) k++;
                    if (k < n && s[k] == '\'') k++;
                    i = k;
                    continue;
                }
                /* Not a full marker: emit from the marker start and advance. */
                { size_t l = (size_t)(hit - s) + 1 - o; memcpy(out + o, s + o, l); o += l; }
                i = (size_t)(hit - s) + 1;
            }
        }
    }
    out[o] = '\0';
    return out;
}

static int fstrip_ws(char c) {
    return c == '\'' || c == '\r' || c == '\n' || c == '\t' || c == ' ';
}
static char *fstrip_quote_ws(const char *s) {
    size_t a = 0, b = strlen(s);
    while (a < b && fstrip_ws(s[a])) a++;
    while (b > a && fstrip_ws(s[b - 1])) b--;
    return foc_dup_n(s + a, b - a);
}

/*
 * PoP: fileopscommon_strip_terminal_fence_leaks @ tools/file_operations_common.py:_strip_terminal_fence_leaks
 */
char *fileopscommon_strip_terminal_fence_leaks(const char *text) {
    const char *p = text ? text : "";
    const char *line = p;
    size_t total = 0, cap = strlen(p) + 16, o = 0;
    char *out = (char *)malloc(cap + 1);
    if (!*p) return foc_dup("");
    if (!out) return foc_dup("");
    while (1) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) + 1 : strlen(line);
        char *piece = foc_dup_n(line, len);
        int had = strstr(piece, "__HERMES_FENCE_") != NULL || strstr(piece, "\x1b]") != NULL;
        char *a = foc_strip_osc(piece);
        char *b = foc_strip_fence_markers(a);
        size_t k, w;
        char *cleaned;
        cleaned = (char *)malloc(strlen(b) + 1);
        if (cleaned) {
            for (k = 0, w = 0; b[k]; k++) if (b[k] != 0x07) cleaned[w++] = b[k];
            cleaned[w] = '\0';
            { char *t = fstrip_quote_ws(cleaned); int empty = !*t; free(t);
              if (!(had && empty)) {
                  size_t cl = strlen(cleaned);
                  if (o + cl + 1 > cap) { /* should not happen: cleaned <= piece */ }
                  memcpy(out + o, cleaned, cl); o += cl; total += cl;
              } }
            free(cleaned);
        }
        free(piece); free(a); free(b);
        if (!end) break;
        line = end + 1;
    }
    out[o] = '\0';
    (void)total;
    return out;
}

/* Decode one UTF-8 code point; returns byte width (1..4). */
static size_t u8dec(const unsigned char *p, unsigned int *cp) {
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0) { *cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
    if ((p[0] & 0xF0) == 0xE0)
        { *cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3; }
    if ((p[0] & 0xF8) == 0xF0)
        { *cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                 ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); return 4; }
    *cp = p[0]; return 1;
}
static int u8_is_Zs(unsigned int c) {
    return c == 0x20 || c == 0xA0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000;
}

/*
 * PoP: fileopscommon_filename_match_key @ tools/file_operations_common.py:filename_match_key
 */
char *fileopscommon_filename_match_key(const char *name) {
    const unsigned char *p = (const unsigned char *)(name ? name : "");
    size_t n = strlen((const char *)p), o = 0;
    char *out = (char *)malloc(n + 8);
    if (!out) return foc_dup("");
    while (*p) {
        unsigned int cp = 0;
        size_t w = u8dec(p, &cp);
        if (cp == 0x2019 || cp == 0x2018) out[o++] = '\'';
        else if (u8_is_Zs(cp)) out[o++] = ' ';
        else { for (size_t k = 0; k < w; k++) out[o++] = (char)p[k]; }
        p += w;
    }
    out[o] = '\0';
    (void)n;
    return out;
}

/*
 * PoP: fileopscommon_detect_line_ending @ tools/file_operations_common.py:_detect_line_ending
 */
const char *fileopscommon_detect_line_ending(const char *sample) {
    const char *p = sample ? sample : "";
    if (!*p) return NULL;
    for (size_t i = 0; i < 4096 && p[i]; i++) {
        if (p[i] == '\r' && p[i + 1] == '\n') return "\r\n";
    }
    for (size_t i = 0; i < 4096 && p[i]; i++) if (p[i] == '\n') return "\n";
    return NULL;
}

/*
 * PoP: fileopscommon_normalize_line_endings @ tools/file_operations_common.py:_normalize_line_endings
 */
char *fileopscommon_normalize_line_endings(const char *text, const char *target) {
    const char *p = text ? text : "";
    if (!target || strcmp(target, "\n") && strcmp(target, "\r\n")) return foc_dup(p);
    {
        size_t n = strlen(p), o = 0;
        char *lf = (char *)malloc(n + 1);
        char *out;
        size_t i = 0;
        if (!lf) return foc_dup(p);
        while (i < n) {
            if (p[i] == '\r') {
                lf[o++] = '\n';
                if (p[i + 1] == '\n') i += 2; else i++;
            } else lf[o++] = p[i++];
        }
        lf[o] = '\0';
        if (!strcmp(target, "\n")) { char *r = foc_dup(lf); free(lf); return r; }
        out = (char *)malloc(o * 2 + 2);
        if (!out) { free(lf); return foc_dup(p); }
        size_t w = 0;
        for (size_t k = 0; k < o; k++) {
            if (lf[k] == '\n') { out[w++] = '\r'; out[w++] = '\n'; }
            else out[w++] = lf[k];
        }
        out[w] = '\0';
        free(lf);
        return out;
    }
}

static int foc_hb(const char *t) {
    return t && (unsigned char)t[0] == BOM0 && (unsigned char)t[1] == BOM1 &&
           (unsigned char)t[2] == BOM2;
}
/*
 * PoP: fileopscommon_has_bom @ tools/file_operations_common.py:_has_bom
 */
int fileopscommon_has_bom(const char *text) { return foc_hb(text) ? 1 : 0; }
/*
 * PoP: fileopscommon_strip_bom @ tools/file_operations_common.py:_strip_bom
 */
int fileopscommon_strip_bom(const char *text, char **out) {
    int had = foc_hb(text) ? 1 : 0;
    const char *p = text ? text : "";
    *out = foc_dup(had ? p + 3 : p);
    return had;
}

static long long foc_int(const char *value, long long def, int *ok) {
    char *end = NULL;
    long long v;
    const char *a, *b;
    char buf[128];
    size_t n;
    if (!value) { if (ok) *ok = 0; return def; }
    a = value; b = value + strlen(value);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    n = (size_t)(b - a);
    if (n == 0 || n >= sizeof(buf)) { if (ok) *ok = 0; return def; }
    memcpy(buf, a, n); buf[n] = '\0';
    v = strtoll(buf, &end, 10);
    if (!end || end == buf || *end != '\0') { if (ok) *ok = 0; return def; }
    if (ok) *ok = 1;
    return v;
}
/*
 * PoP: fileopscommon_coerce_int @ tools/file_operations_common.py:_coerce_int
 */
long long fileopscommon_coerce_int(const char *value, long long def, int *ok) {
    return foc_int(value, def, ok);
}

/*
 * PoP: fileopscommon_normalize_read_pagination @ tools/file_operations_common.py:normalize_read_pagination
 */
void fileopscommon_normalize_read_pagination(const char *offset, const char *limit,
                                             long long max_lines,
                                             long long *out_off, long long *out_lim) {
    long long o = foc_int(offset, 1, NULL);
    long long l = foc_int(limit, 2000, NULL);
    if (o < 1) o = 1;
    if (l > max_lines) l = max_lines;
    if (l < 1) l = 1;
    *out_off = o; *out_lim = l;
}
/*
 * PoP: fileopscommon_normalize_search_pagination @ tools/file_operations_common.py:normalize_search_pagination
 */
void fileopscommon_normalize_search_pagination(const char *offset, const char *limit,
                                               long long *out_off, long long *out_lim) {
    long long o = foc_int(offset, 0, NULL);
    long long l = foc_int(limit, 50, NULL);
    *out_off = o < 0 ? 0 : o;
    *out_lim = l < 1 ? 1 : l;
}

/*
 * PoP: fileopscommon_lint_result_to_dict @ tools/file_operations_common.py:LintResult.to_dict
 */
json_t *fileopscommon_lint_result_to_dict(int success, int skipped,
                                          const char *output, const char *message) {
    json_t *r = json_object();
    if (skipped) {
        json_set(r, "status", json_string("skipped"));
        json_set(r, "message", json_string(message ? message : ""));
        return r;
    }
    json_set(r, "status", json_string(success ? "ok" : "error"));
    json_set(r, "output", json_string(output ? output : ""));
    if (message && *message) json_set(r, "message", json_string(message));
    return r;
}
