/*
 * fuzzy_match.c — Multi-strategy fuzzy matching for find-and-replace.
 *
 * Port of Python tools/fuzzy_match.py (703 lines).
 *
 * Implements an 8-strategy matching chain to robustly find and replace
 * text, accommodating variations in whitespace, indentation, and escaping
 * common in LLM-generated code.
 */

#include "fuzzy_match.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── Constants ──────────────────────────────────────────────────────── */

#define MAX_LINES 4096
#define MAX_LINE_LEN 4096
#define BLOCK_ANCHOR_RATIO 0.6
#define CONTEXT_RATIO 0.5

/* ─── Helpers ────────────────────────────────────────────────────────── */

/* Safe strdup */
static char *sstrdup(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

/* Count lines in a string */
int count_lines(const char *s)
{
    if (!s || !*s) return 0;
    int n = 1;
    while (*s) { if (*s++ == '\n') n++; }
    return n;
}

/* Split a string into lines (into pre-allocated array of MAX_LINES). Returns count. */
static int split_lines(const char *text, const char **lines, int max)
{
    if (!text || !*text) return 0;
    int count = 0;
    const char *start = text;
    while (*start && count < max) {
        const char *end = strchr(start, '\n');
        if (!end) {
            lines[count++] = start;
            break;
        }
        /* Include the newline in the line */
        lines[count++] = start;
        start = end + 1;
    }
    return count;
}

/* Join an array of strings (with separators) into a malloc'd string */
static char *join_strings(const char **parts, int count, const char *sep)
{
    if (count <= 0 || !parts) return sstrdup("");
    size_t total = 0;
    size_t seplen = sep ? strlen(sep) : 0;
    for (int i = 0; i < count; i++)
        total += parts[i] ? strlen(parts[i]) : 0;
    total += (size_t)(count - 1) * seplen + 1;

    char *buf = (char *)malloc(total);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0 && seplen) strcat(buf, sep);
        if (parts[i]) strcat(buf, parts[i]);
    }
    return buf;
}

/* Trim leading and trailing whitespace from a string (returns pointer into original) */
static const char *trim_str(const char *s)
{
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    /* Return a malloc'd substring */
    size_t len = (size_t)(end - s + 1);
    char *trimmed = (char *)malloc(len + 1);
    if (!trimmed) return s;
    memcpy(trimmed, s, len);
    trimmed[len] = '\0';
    return trimmed;
}

/* Trim leading whitespace only (returns pointer into original) */
static const char *trim_left(const char *s)
{
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* Trim trailing whitespace only — returns malloc'd copy */
char *trim_right(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) end--;
    size_t len = (size_t)(end - s + 1);
    char *result = (char *)malloc(len + 1);
    if (!result) return sstrdup(s);
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

/* Check if string contains substring */
static bool contains(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;
    return strstr(haystack, needle) != NULL;
}

/* Replace first occurrence of 'from' with 'to' in 'str' (in-place, str must be malloc'd).
 * Returns new malloc'd string. */
static char *str_replace_first(const char *str, const char *from, const char *to)
{
    if (!str || !from || !*from) return sstrdup(str);
    const char *pos = strstr(str, from);
    if (!pos) return sstrdup(str);

    size_t prefix_len = (size_t)(pos - str);
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    size_t total = prefix_len + to_len + strlen(pos + from_len) + 1;

    char *result = (char *)malloc(total);
    if (!result) return NULL;
    memcpy(result, str, prefix_len);
    memcpy(result + prefix_len, to, to_len);
    strcpy(result + prefix_len + to_len, pos + from_len);
    return result;
}

/* Replace all occurrences — returns malloc'd string */
static char *str_replace_all(const char *str, const char *from, const char *to)
{
    if (!str || !from || !*from) return sstrdup(str);

    /* Count occurrences */
    int count = 0;
    const char *p = str;
    size_t from_len = strlen(from);
    while ((p = strstr(p, from)) != NULL) {
        count++;
        p += from_len;
    }
    if (count == 0) return sstrdup(str);

    /* Build result */
    size_t to_len = strlen(to);
    size_t total = strlen(str) + (size_t)count * (to_len - from_len) + 1;
    char *result = (char *)malloc(total);
    if (!result) return NULL;

    char *dst = result;
    const char *src = str;
    for (int i = 0; i < count; i++) {
        const char *pos = strstr(src, from);
        size_t chunk = (size_t)(pos - src);
        memcpy(dst, src, chunk);
        dst += chunk;
        memcpy(dst, to, to_len);
        dst += to_len;
        src = pos + from_len;
    }
    strcpy(dst, src);
    return result;
}

/* Normalize whitespace: collapse multiple spaces/tabs to single space */
static char *normalize_ws(const char *s)
{
    if (!s || !*s) return sstrdup("");
    char *out = (char *)malloc(strlen(s) + 1);
    if (!out) return NULL;
    size_t j = 0;
    bool was_space = false;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == ' ' || s[i] == '\t') {
            if (!was_space) {
                out[j++] = ' ';
                was_space = true;
            }
        } else {
            /* Keep newlines as-is */
            out[j++] = s[i];
            was_space = false;
        }
    }
    out[j] = '\0';
    return out;
}

/* Strip indentation per line (remove leading whitespace) */
static char *strip_indent(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *lines[MAX_LINES];
    int lc = split_lines(s, lines, MAX_LINES);
    if (lc == 0) return sstrdup("");

    char *stripped[MAX_LINES];
    for (int i = 0; i < lc; i++) {
        stripped[i] = sstrdup(trim_left(lines[i]));
    }
    char *result = join_strings((const char **)stripped, lc, "");
    for (int i = 0; i < lc; i++) free(stripped[i]);
    return result;
}

/* Trim each line */
static char *trim_each_line(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *lines[MAX_LINES];
    int lc = split_lines(s, lines, MAX_LINES);
    if (lc == 0) return sstrdup("");

    /* Python: "\n".join(line.strip() for line in s.split("\n")).
     * split_lines() hands back POINTERS into s whose strlen() runs past the
     * line, so bound each line by the next line's start (minus its '\n'). */
    size_t cap = strlen(s) + 2;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    size_t o = 0;
    for (int i = 0; i < lc; i++) {
        const char *ln = lines[i];
        size_t body;
        int had_nl;
        if (i + 1 < lc) {
            body = (size_t)(lines[i + 1] - ln);
            had_nl = (body > 0 && ln[body - 1] == '\n');
            body = had_nl ? body - 1 : body;
        } else {
            body = strlen(ln);
            had_nl = 0;
        }
        size_t a = 0, b = body;
        while (a < b && isspace((unsigned char)ln[a])) a++;
        while (b > a && isspace((unsigned char)ln[b - 1])) b--;
        if (o + (b - a) + 2 >= cap) {
            cap = (o + (b - a) + 2) * 2;
            char *n = (char *)realloc(result, cap);
            if (!n) { free(result); return NULL; }
            result = n;
        }
        memcpy(result + o, ln + a, b - a);
        o += b - a;
        if (had_nl) result[o++] = '\n';
    }
    result[o] = '\0';
    return result;
}

/* Trim first and last line boundaries only */
static char *trim_boundary(const char *s)
{
    if (!s || !*s) return sstrdup("");
    const char *lines[MAX_LINES];
    int lc = split_lines(s, lines, MAX_LINES);
    if (lc == 0) return sstrdup("");

    /* Trim ONLY the boundary lines' outer whitespace; middle lines pass
     * through verbatim (python _strategy_trimmed_boundary). Bound each
     * line by the next line's start (split_lines pointers overrun). */
    size_t cap = strlen(s) + 2;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    size_t o = 0;
    for (int i = 0; i < lc; i++) {
        const char *ln = lines[i];
        size_t body;
        int had_nl;
        if (i + 1 < lc) {
            body = (size_t)(lines[i + 1] - ln);
            had_nl = (body > 0 && ln[body - 1] == '\n');
            body = had_nl ? body - 1 : body;
        } else {
            body = strlen(ln);
            had_nl = 0;
        }
        size_t a = 0, b = body;
        if (i == 0 || i == lc - 1) {
            while (a < b && isspace((unsigned char)ln[a])) a++;
            while (b > a && isspace((unsigned char)ln[b - 1])) b--;
        }
        if (o + (b - a) + 2 >= cap) {
            cap = (o + (b - a) + 2) * 2;
            char *n = (char *)realloc(result, cap);
            if (!n) { free(result); return NULL; }
            result = n;
        }
        memcpy(result + o, ln + a, b - a);
        o += b - a;
        if (had_nl) result[o++] = '\n';
    }
    result[o] = '\0';
    return result;
}

/* Escape-normalize: convert literal '\n' to actual newlines */
static char *normalize_escapes(const char *s)
{
    if (!s || !*s) return sstrdup("");
    return str_replace_all(s, "\\n", "\n");
}

/* ─── Similarity ratio (line-based LCS) ─────────────────────────────── */

double fuzzy_ratio(const char *a, const char *b)
{
    if (!a && !b) return 1.0;
    if (!a || !b) return 0.0;

    const char *lines_a[MAX_LINES], *lines_b[MAX_LINES];
    int lc_a = split_lines(a, lines_a, MAX_LINES);
    int lc_b = split_lines(b, lines_b, MAX_LINES);

    if (lc_a == 0 && lc_b == 0) return 1.0;
    if (lc_a == 0 || lc_b == 0) return 0.0;

    /* LCS on line level */
    int **dp = (int **)calloc((size_t)(lc_a + 1), sizeof(int *));
    if (!dp) return 0.0;
    for (int i = 0; i <= lc_a; i++) {
        dp[i] = (int *)calloc((size_t)(lc_b + 1), sizeof(int));
        if (!dp[i]) {
            for (int j = 0; j < i; j++) free(dp[j]);
            free(dp);
            return 0.0;
        }
    }

    /* LCS on line level — need to compare per-line, not full substrings */
    /* Extract each line as a proper null-terminated string */
    char line_a[4096], line_b[4096];

    for (int i = 1; i <= lc_a; i++) {
        for (int j = 1; j <= lc_b; j++) {
            /* Extract first line from each (up to newline or end) */
            const char *pa = lines_a[i-1];
            const char *pb = lines_b[j-1];
            int la = 0, lb = 0;
            while (pa && pa[la] && pa[la] != '\n' && la < 4095) {
                line_a[la] = pa[la]; la++;
            }
            line_a[la] = '\0';
            while (pb && pb[lb] && pb[lb] != '\n' && lb < 4095) {
                line_b[lb] = pb[lb]; lb++;
            }
            line_b[lb] = '\0';

            bool match = (la == lb) && memcmp(line_a, line_b, la) == 0;
            if (match)
                dp[i][j] = dp[i-1][j-1] + 1;
            else
                dp[i][j] = (dp[i-1][j] > dp[i][j-1]) ? dp[i-1][j] : dp[i][j-1];
        }
    }

    int lcs = dp[lc_a][lc_b];
    for (int i = 0; i <= lc_a; i++) free(dp[i]);
    free(dp);

    int max_lines = (lc_a > lc_b) ? lc_a : lc_b;
    return (double)lcs / (double)max_lines;
}

/* ─── Strategy implementations ──────────────────────────────────────── */

/*
 * Each strategy function takes: content, old_string, new_string, replace_all.
 * Returns: fuzzy_result_t (caller must free content + error).
 *
 * Returns NULL if this strategy doesn't match (try next).
 */

/* Port of Python tools/fuzzy_match.py:_strategy_exact(). */
/* Strategy 1: Exact match */
static fuzzy_result_t *strategy_exact(const char *content, const char *old_string,
                                       const char *new_string, bool replace_all)
{
    if (!content || !old_string || !new_string) return NULL;
    if (!contains(content, old_string)) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) return NULL;

    if (replace_all)
        r->content = str_replace_all(content, old_string, new_string);
    else
        r->content = str_replace_first(content, old_string, new_string);

    if (!r->content) { free(r); return NULL; }

    /* Count matches (safely handle empty old_string) */
    int count = 0;
    if (strlen(old_string) > 0) {
        const char *p = content;
        size_t olen = strlen(old_string);
        while ((p = strstr(p, old_string)) != NULL) { count++; p += olen; }
    }
    r->match_count = count;
    r->strategy = 1;
    return r;
}

/* Port of Python tools/fuzzy_match.py:_strategy_line_trimmed(). */
/* Strategy 2: Line-trimmed */
static fuzzy_result_t *strategy_line_trimmed(const char *content,
                                              const char *old_string,
                                              const char *new_string,
                                              bool replace_all)
{
    char *trimmed_content = trim_each_line(content);
    char *trimmed_old = trim_each_line(old_string);
    if (!trimmed_content || !trimmed_old) {
        free(trimmed_content); free(trimmed_old);
        return NULL;
    }

    if (!contains(trimmed_content, trimmed_old)) {
        free(trimmed_content); free(trimmed_old);
        return NULL;
    }

    /* Replace in trimmed content */
    char *replaced;
    if (replace_all)
        replaced = str_replace_all(trimmed_content, trimmed_old, new_string);
    else
        replaced = str_replace_first(trimmed_content, trimmed_old, new_string);

    free(trimmed_content);
    free(trimmed_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 2;
    return r;
}

/* Strategy 3: Whitespace normalized */
static fuzzy_result_t *strategy_ws_normalized(const char *content,
                                               const char *old_string,
                                               const char *new_string,
                                               bool replace_all)
{
    char *norm_content = normalize_ws(content);
    char *norm_old = normalize_ws(old_string);
    if (!norm_content || !norm_old) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    if (!contains(norm_content, norm_old)) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(norm_content, norm_old, new_string);
    else
        replaced = str_replace_first(norm_content, norm_old, new_string);

    free(norm_content);
    free(norm_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 3;
    return r;
}

/* Strategy 4: Indentation flexible */
static fuzzy_result_t *strategy_indent_free(const char *content,
                                             const char *old_string,
                                             const char *new_string,
                                             bool replace_all)
{
    char *strip_content = strip_indent(content);
    char *strip_old = strip_indent(old_string);
    if (!strip_content || !strip_old) {
        free(strip_content); free(strip_old);
        return NULL;
    }

    if (!contains(strip_content, strip_old)) {
        free(strip_content); free(strip_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(strip_content, strip_old, new_string);
    else
        replaced = str_replace_first(strip_content, strip_old, new_string);

    free(strip_content);
    free(strip_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 4;
    return r;
}

/* Strategy 5: Escape normalized */
static fuzzy_result_t *strategy_escape_norm(const char *content,
                                             const char *old_string,
                                             const char *new_string,
                                             bool replace_all)
{
    char *norm_content = normalize_escapes(content);
    char *norm_old = normalize_escapes(old_string);
    if (!norm_content || !norm_old) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    if (!contains(norm_content, norm_old)) {
        free(norm_content); free(norm_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(norm_content, norm_old, new_string);
    else
        replaced = str_replace_first(norm_content, norm_old, new_string);

    free(norm_content);
    free(norm_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 5;
    return r;
}

/* Strategy 6: Trimmed boundary */
static fuzzy_result_t *strategy_trim_boundary(const char *content,
                                               const char *old_string,
                                               const char *new_string,
                                               bool replace_all)
{
    char *trim_content = trim_boundary(content);
    char *trim_old = trim_boundary(old_string);
    if (!trim_content || !trim_old) {
        free(trim_content); free(trim_old);
        return NULL;
    }

    if (!contains(trim_content, trim_old)) {
        free(trim_content); free(trim_old);
        return NULL;
    }

    char *replaced;
    if (replace_all)
        replaced = str_replace_all(trim_content, trim_old, new_string);
    else
        replaced = str_replace_first(trim_content, trim_old, new_string);

    free(trim_content);
    free(trim_old);

    if (!replaced) return NULL;

    fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
    if (!r) { free(replaced); return NULL; }
    r->content = replaced;
    r->match_count = 1;
    r->strategy = 6;
    return r;
}

/* ─── difflib.SequenceMatcher-compatible ratio ───────────────────────── */

static int sm_find_longest_match(const unsigned char *a, size_t alo, size_t ahi,
                                 const unsigned char *b, size_t blo, size_t bhi,
                                 size_t *best_i, size_t *best_j)
{
    size_t n = bhi - blo;
    if (n == 0 || ahi <= alo) { *best_i = alo; *best_j = blo; return 0; }
    size_t *dp = (size_t *)calloc(n + 1, sizeof(size_t));   /* current row */
    size_t *prev = (size_t *)calloc(n + 1, sizeof(size_t)); /* previous row */
    size_t bestlen = 0;
    *best_i = alo; *best_j = blo;
    for (size_t i = alo; i < ahi; i++) {
        memcpy(prev, dp, (n + 1) * sizeof(size_t));
        memset(dp, 0, (n + 1) * sizeof(size_t));
        for (size_t j = 0; j < n; j++) {
            if (a[i] == b[blo + j]) {
                size_t k = (j == 0) ? 1 : prev[j] + 1;   /* prev row, col j-1 */
                /* careful: prev[j-1] is previous row's value at column j-1 */
                k = (j == 0) ? 1 : prev[j - 1] + 1;
                dp[j] = k;
                if (k > bestlen) {
                    bestlen = k;
                    *best_i = i + 1 - k;
                    *best_j = blo + j + 1 - k;
                }
            } else {
                dp[j] = 0;
            }
        }
    }
    free(dp); free(prev);
    return (int)bestlen;
}

static void sm_accum(const unsigned char *a, size_t alo, size_t ahi,
                     const unsigned char *b, size_t blo, size_t bhi,
                     size_t *matched)
{
    if (alo >= ahi || blo >= bhi) return;
    size_t bi, bj;
    int k = sm_find_longest_match(a, alo, ahi, b, blo, bhi, &bi, &bj);
    if (k > 0) {
        *matched += (size_t)k;
        sm_accum(a, alo, bi, b, blo, bj, matched);
        sm_accum(a, bi + (size_t)k, ahi, b, bj + (size_t)k, bhi, matched);
    }
}

/* difflib SequenceMatcher(None, a, b).ratio() (autojunk irrelevant <200). */
double fuzzy_sm_ratio(const char *x, const char *y)
{
    if (!x) x = "";
    if (!y) y = "";
    size_t lx = strlen(x), ly = strlen(y);
    if (lx == 0 && ly == 0) return 1.0;
    if (lx == 0 || ly == 0) return 0.0;
    size_t matched = 0;
    sm_accum((const unsigned char *)x, 0, lx,
             (const unsigned char *)y, 0, ly, &matched);
    return (2.0 * (double)matched) / (double)(lx + ly);
}

/* split into malloc'd lines WITHOUT newlines (python s.split("\n")) */
static int split_lines_bare(const char *s, char **out, int max)
{
    if (!s) return 0;
    int count = 0;
    const char *start = s;
    for (;;) {
        const char *nl = strchr(start, '\n');
        size_t len = nl ? (size_t)(nl - start) : strlen(start);
        if (count < max) {
            out[count] = (char *)malloc(len + 1);
            if (out[count]) {
                memcpy(out[count], start, len);
                out[count][len] = '\0';
            }
            count++;
        }
        if (!nl) break;
        start = nl + 1;
    }
    return count;
}

/* trim into a caller buffer (avoids trim_str's mixed ownership) */
static void trim_into(const char *s, char *out, size_t cap)
{
    if (!s || cap == 0) { if (cap) out[0] = '\0'; return; }
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    size_t len = strlen(a);
    while (len > 0 && isspace((unsigned char)a[len - 1])) len--;
    if (len >= cap) len = cap - 1;
    memcpy(out, a, len);
    out[len] = '\0';
}

static void free_lines(char **lines, int n)
{
    for (int i = 0; i < n; i++) free(lines[i]);
}

/* Port of Python tools/fuzzy_match.py:_strategy_block_anchor(). */
/* Strategy 7: Block anchor — match first+last line, ratio check on middle */
static fuzzy_result_t *strategy_block_anchor(const char *content,
                                              const char *old_string,
                                              const char *new_string,
                                              bool replace_all)
{
    (void)replace_all; /* Block anchor only does first match */
    if (!content || !old_string || !new_string) return NULL;

    char *c_lines[MAX_LINES], *p_lines[MAX_LINES];
    int c_lc = split_lines_bare(content, c_lines, MAX_LINES);
    int p_lc = split_lines_bare(old_string, p_lines, MAX_LINES);
    /* python: pattern_lines = norm_pattern.split('\n'); need >= 2 lines */
    if (p_lc < 2) { free_lines(c_lines, c_lc); free_lines(p_lines, p_lc); return NULL; }

    const char *first_line = p_lines[0];
    const char *last_line  = p_lines[p_lc - 1];

    /* windows whose stripped first/last lines anchor exactly */
    int potential[MAX_LINES];
    int candidate_count = 0;
    for (int i = 0; i + p_lc <= c_lc; i++) {
        char fs[1024], ls[1024];
        trim_into(c_lines[i], fs, sizeof fs);
        trim_into(c_lines[i + p_lc - 1], ls, sizeof ls);
        if (strcmp(fs, first_line) == 0 && strcmp(ls, last_line) == 0) {
            if (candidate_count < MAX_LINES) potential[candidate_count] = i;
            candidate_count++;
        }
    }

    double threshold = (candidate_count == 1) ? 0.50 : 0.70;

    for (int w = 0; w < candidate_count; w++) {
        int i = potential[w];
        double similarity;
        if (p_lc <= 2) {
            similarity = 1.0;
        } else {
            /* join middles with newline and compare with difflib ratio */
            size_t mc_cap = 4096, mp_cap = 4096;
            char *mc = (char *)malloc(mc_cap), *mp = (char *)malloc(mp_cap);
            if (!mc || !mp) { free(mc); free(mp); break; }
            mc[0] = mp[0] = '\0';
            size_t mo = 0, po = 0;
            for (int k = 1; k < p_lc - 1 && k < p_lc; k++) {
                if (mo + strlen(c_lines[i + k]) + 2 < mc_cap) {
                    if (mo) mc[mo++] = '\n';
                    strcpy(mc + mo, c_lines[i + k]); mo += strlen(c_lines[i + k]);
                }
                if (po + strlen(p_lines[k]) + 2 < mp_cap) {
                    if (po) mp[po++] = '\n';
                    strcpy(mp + po, p_lines[k]); po += strlen(p_lines[k]);
                }
            }
            similarity = fuzzy_sm_ratio(mc, mp);
            free(mc); free(mp);
        }
        if (similarity >= threshold) {
            /* replace content lines [i, i+p_lc) with new_string */
            size_t pre = 0;
            for (int k = 0; k < i; k++) pre += strlen(c_lines[k]) + 1;
            size_t post = 0;
            for (int k = i + p_lc; k < c_lc; k++) post += strlen(c_lines[k]) + 1;
            char *result = (char *)malloc(pre + strlen(new_string) + post + 2);
            if (!result) break;
            size_t o = 0;
            for (int k = 0; k < i; k++) {
                memcpy(result + o, c_lines[k], strlen(c_lines[k]));
                o += strlen(c_lines[k]);
                result[o++] = '\n';
            }
            memcpy(result + o, new_string, strlen(new_string));
            o += strlen(new_string);
            for (int k = i + p_lc; k < c_lc; k++) {
                result[o++] = '\n';
                memcpy(result + o, c_lines[k], strlen(c_lines[k]));
                o += strlen(c_lines[k]);
            }
            result[o] = '\0';
            free_lines(c_lines, c_lc);
            free_lines(p_lines, p_lc);
            fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
            if (!r) { free(result); return NULL; }
            r->content = result;
            r->match_count = 1;
            r->strategy = 8;
            return r;
        }
    }
    free_lines(c_lines, c_lc);
    free_lines(p_lines, p_lc);
    return NULL;
}

/* Strategy 9: Context-aware — anchored line-by-line similarity (python
 * _strategy_context_aware: anchor pre-filter on first/last lines at 0.80,
 * then EVERY non-blank pattern line must be >=0.80 similar to its aligned
 * content line). */
static fuzzy_result_t *strategy_context(const char *content,
                                         const char *old_string,
                                         const char *new_string,
                                         bool replace_all)
{
    (void)replace_all; /* Only first match for context */
    if (!content || !old_string || !new_string) return NULL;

    char *c_lines[MAX_LINES], *p_lines[MAX_LINES];
    int c_lc = split_lines_bare(content, c_lines, MAX_LINES);
    int p_lc = split_lines_bare(old_string, p_lines, MAX_LINES);
    if (p_lc < 1 || c_lc < p_lc) {
        free_lines(c_lines, c_lc); free_lines(p_lines, p_lc);
        return NULL;
    }

    char first_pat[1024], last_pat[1024];
    {
        char first_pat[1024];
        trim_into(p_lines[0], first_pat, sizeof first_pat);
        trim_into(p_lines[p_lc - 1], last_pat, sizeof last_pat);
    }

    for (int i = 0; i + p_lc <= c_lc; i++) {
        char b_first[1024], b_last[1024];
        {
            char b_first[1024];
            trim_into(c_lines[i], b_first, sizeof b_first);
            trim_into(c_lines[i + p_lc - 1], b_last, sizeof b_last);
        }
        if (strcmp(first_pat, b_first) == 0) {
            if (fuzzy_sm_ratio(first_pat, b_first) < 0.80) continue;
        } else if (fuzzy_sm_ratio(first_pat, b_first) < 0.80) {
            continue;
        }
        if (fuzzy_sm_ratio(last_pat, b_last) < 0.80) continue;

        int all_match = 1;
        for (int j = 0; j < p_lc; j++) {
            char ptb[1024], ctb[1024];
            trim_into(p_lines[j], ptb, sizeof ptb);
            trim_into(c_lines[i + j], ctb, sizeof ctb);
            if (!ptb[0]) continue;
            double sim = fuzzy_sm_ratio(ptb, ctb);
            if (sim < 0.80) { all_match = 0; break; }
        }
        if (all_match) {
            size_t pre = 0;
            for (int k = 0; k < i; k++) pre += strlen(c_lines[k]) + 1;
            size_t post = 0;
            for (int k = i + p_lc; k < c_lc; k++) post += strlen(c_lines[k]) + 1;
            char *result = (char *)malloc(pre + strlen(new_string) + post + 2);
            if (!result) break;
            size_t o = 0;
            for (int k = 0; k < i; k++) {
                memcpy(result + o, c_lines[k], strlen(c_lines[k]));
                o += strlen(c_lines[k]);
                result[o++] = '\n';
            }
            memcpy(result + o, new_string, strlen(new_string));
            o += strlen(new_string);
            for (int k = i + p_lc; k < c_lc; k++) {
                result[o++] = '\n';
                memcpy(result + o, c_lines[k], strlen(c_lines[k]));
                o += strlen(c_lines[k]);
            }
            result[o] = '\0';
            free_lines(c_lines, c_lc);
            free_lines(p_lines, p_lc);
            fuzzy_result_t *r = (fuzzy_result_t *)calloc(1, sizeof(fuzzy_result_t));
            if (!r) { free(result); return NULL; }
            r->content = result;
            r->match_count = 1;
            r->strategy = 9;
            return r;
        }
    }
    free_lines(c_lines, c_lc);
    free_lines(p_lines, p_lc);
    return NULL;
}

/* ─── Strategy function pointer table ────────────────────────────────── */

typedef fuzzy_result_t *(*strategy_fn)(const char *, const char *,
                                        const char *, bool);

static const strategy_fn STRATEGIES[] = {
    strategy_exact,
    strategy_line_trimmed,
    strategy_ws_normalized,
    strategy_indent_free,
    strategy_escape_norm,
    strategy_trim_boundary,
    strategy_block_anchor,
    strategy_context,
};

#define NUM_STRATEGIES (sizeof(STRATEGIES) / sizeof(STRATEGIES[0]))

/* ─── Public API ─────────────────────────────────────────────────────── */

fuzzy_result_t fuzzy_find_and_replace(const char *content,
                                       const char *old_string,
                                       const char *new_string,
                                       bool replace_all)
{
    fuzzy_result_t null_result = {NULL, 0, 0, NULL};

    if (!content || !old_string || !new_string) {
        null_result.error = sstrdup("NULL argument");
        return null_result;
    }
    /* LIVE guards (run before the strategy chain):
     *   "" old            -> "old_string cannot be empty"
     *   whitespace-only   -> "old_string is only whitespace ..."
     *   old == new        -> IDENTICAL_STRINGS_ERROR */
    if (!*old_string) {
        null_result.error = sstrdup("old_string cannot be empty");
        return null_result;
    }
    {
        int only_ws = 1;
        for (const char *q = old_string; *q; q++)
            if (!isspace((unsigned char)*q)) { only_ws = 0; break; }
        if (only_ws) {
            null_result.error = sstrdup(
                "old_string is only whitespace \u2014 provide non-blank text to match");
            return null_result;
        }
    }
    if (strcmp(old_string, new_string) == 0) {
        null_result.error = sstrdup(
            "No edit was applied because old_string and new_string are identical. "
            "Provide the existing text to replace in old_string and the changed "
            "replacement text in new_string.");
        return null_result;
    }

    for (size_t i = 0; i < NUM_STRATEGIES; i++) {
        fuzzy_result_t *r = STRATEGIES[i](content, old_string, new_string, replace_all);
        if (r) {
            fuzzy_result_t result = *r;
            free(r);
            return result;
        }
    }

    null_result.error = sstrdup("no match found");
    return null_result;
}

void fuzzy_result_free(fuzzy_result_t *result)
{
    if (!result) return;
    free(result->content);
    free(result->error);
    result->content = NULL;
    result->error = NULL;
    result->match_count = 0;
    result->strategy = 0;
}
