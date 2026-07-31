/*
 * patch.c — Patch/find-replace tool for Hermes C.
 * Port of Python tools/patch_parser.py.
 * Reads file, finds unique old_string, replaces with new_string.
 * Supports: replace_all mode, fuzzy matching (basic), diff output.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "patch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Schema */
static const char *SCHEMA_PATCH = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"mode\":{\"type\":\"string\",\"description\":\"replace (find+replace) or patch (V4A multi-file format)\",\"default\":\"replace\"},"
      "\"path\":{\"type\":\"string\",\"description\":\"File path to edit (required for replace mode)\"},"
      "\"old_string\":{\"type\":\"string\",\"description\":\"Text to find (must be unique unless replace_all=true)\"},"
      "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement text\"},"
      "\"replace_all\":{\"type\":\"boolean\",\"description\":\"Replace all occurrences\",\"default\":false},"
      "\"dry_run\":{\"type\":\"boolean\",\"description\":\"Preview changes without modifying the file. Returns the same diff output but skips the actual file write.\",\"default\":false},"
      "\"patch\":{\"type\":\"string\",\"description\":\"V4A patch content (required for patch mode): *** Begin Patch ... *** End Patch\"}"
    "},"
    "\"required\":[]"
"}";

/* ================================================================
 *  Fuzzy matching strategies (ported from Python fuzzy_match.py)
 * ================================================================ */

/* Helper: count lines in a string */
static int _count_lines(const char *s) {
    int n = 1;
    while (*s) { if (*s++ == '\n') n++; }
    return n;
}

/* Helper: duplicate a string and replace one char with another */
static char *_str_replace_char(const char *s, char from, char to) {
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) return NULL;
    for (char *p = dup; *p; p++) {
        if (*p == from) *p = to;
    }
    return dup;
}

/* PoP: fuzzy_find_and_replace @ fuzzy_match:fuzzy_find_and_replace */
/* Strategy 1: Exact match (returns 1 if found, 0 if not).
 * Sets *match_start = position in content, *match_len = pattern length. */
static int _fuzzy_exact(const char *content, const char *pattern,
                         size_t *match_start, size_t *match_len)
{
    const char *p = strstr(content, pattern);
    if (!p) return 0;
    *match_start = (size_t)(p - content);
    *match_len = strlen(pattern);
    return 1;
}

/* Strategy 2: Line-trimmed — strip leading/trailing whitespace per line,
 * then find as sequence of trimmed lines in trimmed content. */
static int _fuzzy_line_trimmed(const char *content, const char *pattern,
                                size_t *match_start, size_t *match_len)
{
    int plines = _count_lines(pattern);
    int clines = _count_lines(content);
    if (plines > clines) return 0;

    /* Split content and pattern into lines (tokenized) */
    /* Use strtok_r — need mutable copies */
    char *content_copy = strdup(content);
    char *pattern_copy = strdup(pattern);
    if (!content_copy || !pattern_copy) { free(content_copy); free(pattern_copy); return 0; }

    /* Build trimmed pattern lines array */
    char *pat_lines[256];
    int npat = 0;
    char *save1 = NULL;
    char *tok = strtok_r(pattern_copy, "\n", &save1);
    while (tok && npat < 256) {
        /* Strip leading/trailing whitespace */
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
        *end = '\0';
        pat_lines[npat++] = tok;
        tok = strtok_r(NULL, "\n", &save1);
    }

    /* Build trimmed content lines array */
    char *con_lines[4096];
    int ncon = 0;
    char *save2 = NULL;
    char *ctok = strtok_r(content_copy, "\n", &save2);
    while (ctok && ncon < 4096) {
        while (*ctok == ' ' || *ctok == '\t') ctok++;
        char *end = ctok + strlen(ctok);
        while (end > ctok && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
        *end = '\0';
        con_lines[ncon++] = ctok;
        ctok = strtok_r(NULL, "\n", &save2);
    }

    /* Find the pattern as a contiguous subsequence in content */
    int found = 0;
    int start_line = 0;
    for (int i = 0; i <= ncon - npat; i++) {
        int match = 1;
        for (int j = 0; j < npat; j++) {
            if (strcmp(con_lines[i + j], pat_lines[j]) != 0) { match = 0; break; }
        }
        if (match) { found = 1; start_line = i; break; }
    }

    free(content_copy);
    free(pattern_copy);

    if (!found) return 0;

    /* Calculate byte positions in original content */
    /* Walk through original content to find the N-th newline */
    const char *ptr = content;
    for (int i = 0; i < start_line && *ptr; i++) {
        ptr = strchr(ptr, '\n');
        if (!ptr) return 0;
        ptr++; /* skip newline */
    }
    *match_start = (size_t)(ptr - content);

    /* Find end: skip npat-1 newlines from start of match */
    const char *end_ptr = content + *match_start;
    for (int i = 0; i < npat; i++) {
        const char *nl = strchr(end_ptr, '\n');
        if (!nl) { end_ptr = content + strlen(content); break; }
        end_ptr = nl + 1;
    }
    *match_len = (size_t)(end_ptr - (content + *match_start));

    return 1;
}

/* Strategy 3: Whitespace normalized — collapse multiple spaces/tabs to single space */
static int _fuzzy_whitespace_normalized(const char *content, const char *pattern,
                                         size_t *match_start, size_t *match_len)
{
    /* Normalize pattern: collapse spaces, tabs to single space, trim */
    char *pat_norm = _str_replace_char(pattern, '\t', ' ');
    if (!pat_norm) return 0;

    /* Collapse multiple spaces in pattern */
    char *pat_collapsed = strdup(pat_norm);
    free(pat_norm);
    if (!pat_collapsed) return 0;
    int w = 0;
    for (int r = 0; pat_collapsed[r]; r++) {
        if (pat_collapsed[r] == ' ' && r > 0 && pat_collapsed[r-1] == ' ') continue;
        pat_collapsed[w++] = pat_collapsed[r];
    }
    pat_collapsed[w] = '\0';

    /* Normalize content: collapse spaces, tabs to single space */
    char *con_norm = _str_replace_char(content, '\t', ' ');
    if (!con_norm) { free(pat_collapsed); return 0; }
    char *con_collapsed = strdup(con_norm);
    free(con_norm);
    if (!con_collapsed) { free(pat_collapsed); return 0; }
    w = 0;
    for (int r = 0; con_collapsed[r]; r++) {
        if (con_collapsed[r] == ' ' && r > 0 && con_collapsed[r-1] == ' ') continue;
        con_collapsed[w++] = con_collapsed[r];
    }
    con_collapsed[w] = '\0';

    /* Build position map: for each byte in con_collapsed, which byte in original content? */
    /* We need to map bytes by walking both in parallel */
    size_t con_len = strlen(content);
    size_t *pos_map = (size_t *)malloc((strlen(con_collapsed) + 1) * sizeof(size_t));
    if (!pos_map) { free(pat_collapsed); free(con_collapsed); return 0; }

    size_t ci = 0, ni = 0;
    while (ci < con_len && ni < strlen(con_collapsed)) {
        pos_map[ni] = ci;
        /* The normalized byte at ni came from content[ci] */
        if (content[ci] == '\t') {
            /* Tab → space: consume 1 tab, emit 1 space */
            ci++; ni++;
        } else if (content[ci] == ' ') {
            /* Consume all consecutive spaces as one */
            ci++;
            while (ci < con_len && (content[ci] == ' ' || content[ci] == '\t')) ci++;
            ni++;
        } else {
            ci++; ni++;
        }
    }
    if (ni > 0) ni--;
    pos_map[ni] = ci; /* fix last */

    /* Try exact match in normalized content */
    const char *np = strstr(con_collapsed, pat_collapsed);
    int result = 0;
    if (np) {
        size_t norm_start = (size_t)(np - con_collapsed);
        size_t norm_end = norm_start + strlen(pat_collapsed);
        *match_start = pos_map[norm_start];
        *match_len = (norm_end <= ni) ? (pos_map[norm_end] - pos_map[norm_start])
                     : (con_len - pos_map[norm_start]);
        result = 1;
    }

    free(pos_map);
    free(pat_collapsed);
    free(con_collapsed);
    return result;
}

/* Strategy 4: Indentation flexible — ignore leading whitespace */
static int _fuzzy_indentation_flexible(const char *content, const char *pattern,
                                        size_t *match_start, size_t *match_len)
{
    /* Same as line_trimmed but only left-strip, not right-strip */
    int plines = _count_lines(pattern);
    int clines = _count_lines(content);
    if (plines > clines) return 0;

    char *content_copy = strdup(content);
    char *pattern_copy = strdup(pattern);
    if (!content_copy || !pattern_copy) { free(content_copy); free(pattern_copy); return 0; }

    char *pat_lines[256]; int npat = 0;
    char *save1 = NULL;
    char *tok = strtok_r(pattern_copy, "\n", &save1);
    while (tok && npat < 256) {
        while (*tok == ' ' || *tok == '\t') tok++;
        pat_lines[npat++] = tok;
        tok = strtok_r(NULL, "\n", &save1);
    }

    char *con_lines[4096]; int ncon = 0;
    char *save2 = NULL;
    char *ctok = strtok_r(content_copy, "\n", &save2);
    while (ctok && ncon < 4096) {
        char *stripped = ctok;
        while (*stripped == ' ' || *stripped == '\t') stripped++;
        con_lines[ncon++] = stripped;
        ctok = strtok_r(NULL, "\n", &save2);
    }

    int found = 0, start_line = 0;
    for (int i = 0; i <= ncon - npat; i++) {
        int match = 1;
        for (int j = 0; j < npat; j++) {
            if (strcmp(con_lines[i + j], pat_lines[j]) != 0) { match = 0; break; }
        }
        if (match) { found = 1; start_line = i; break; }
    }

    free(content_copy);
    free(pattern_copy);

    if (!found) return 0;

    const char *ptr = content;
    for (int i = 0; i < start_line && *ptr; i++) {
        ptr = strchr(ptr, '\n');
        if (!ptr) return 0;
        ptr++;
    }
    *match_start = (size_t)(ptr - content);

    const char *end_ptr = content + *match_start;
    for (int i = 0; i < npat; i++) {
        const char *nl = strchr(end_ptr, '\n');
        if (!nl) { end_ptr = content + strlen(content); break; }
        end_ptr = nl + 1;
    }
    *match_len = (size_t)(end_ptr - (content + *match_start));

    return 1;
}

/* ---------------------------------------------------------------------------
 *  Strategy 5-9 helpers + additional fuzzy strategies (port of fuzzy_match.py
 *  strategies 5-9: escape_normalized, trimmed_boundary, unicode_normalized,
 *  block_anchor, context_aware). Also the escape-drift guard and the
 *  indent-realignment of new_string, both of which Python applies after a
 *  non-exact fuzzy match so the edit lands correctly on disk.
 * ------------------------------------------------------------------------- */

/* Line-similarity ratio approximating difflib.SequenceMatcher.ratio() on two
 * strings: 2*M/(len_a+len_b) where M is the longest-common-subsequence length
 * (computed via LCS on the two char sequences). Lines are short, so O(n^2) is
 * fine. Returns a double in [0,1]. */
static double _line_sim(const char *a, const char *b)
{
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la == 0 && lb == 0) return 1.0;
    if (la == 0 || lb == 0) return 0.0;
    int dp[la + 1][lb + 1];
    for (int i = 0; i <= la; i++) dp[i][0] = 0;
    for (int j = 0; j <= lb; j++) dp[0][j] = 0;
    for (int i = 1; i <= la; i++)
        for (int j = 1; j <= lb; j++)
            dp[i][j] = (a[i - 1] == b[j - 1]) ? dp[i - 1][j - 1] + 1
                                             : (dp[i - 1][j] > dp[i][j - 1] ? dp[i - 1][j] : dp[i][j - 1]);
    double m = (double)dp[la][lb];
    return 2.0 * m / (double)(la + lb);
}

/* Calculate start/end byte positions of a line span [start_line, end_line)
 * (0-based, end exclusive) in content. Mirrors _calculate_line_positions. */
static void _line_positions(const char *content, int start_line, int end_line,
                            size_t *start, size_t *end, size_t content_len)
{
    size_t sp = 0;
    const char *p = content;
    for (int i = 0; i < start_line && *p; i++) {
        const char *nl = strchr(p, '\n');
        if (!nl) { sp = content_len; *start = *end = content_len; return; }
        sp = (size_t)(nl - content) + 1;
        p = nl + 1;
    }
    *start = sp;
    size_t ep = 0;
    const char *q = content;
    for (int i = 0; i < end_line && *q; i++) {
        const char *nl = strchr(q, '\n');
        if (!nl) { ep = content_len; *end = content_len; return; }
        ep = (size_t)(nl - content) + 1;
        q = nl + 1;
    }
    *end = (ep > 0) ? (ep - 1) : 0;
    if (*end > content_len) *end = content_len;
}

/* Strategy 5: escape_normalized — convert \\n \\t \\r literals in the PATTERN to
 * real control characters, then exact-match. Skip if pattern has no escapes. */
static int _fuzzy_escape_normalized(const char *content, const char *pattern,
                                    size_t *match_start, size_t *match_len)
{
    if (!strstr(pattern, "\\n") && !strstr(pattern, "\\t") && !strstr(pattern, "\\r"))
        return 0;
    /* Build unescaped pattern */
    size_t plen = strlen(pattern);
    char *unesc = (char *)malloc(plen + 1);
    if (!unesc) return 0;
    size_t k = 0;
    for (size_t i = 0; i < plen; i++) {
        if (pattern[i] == '\\' && i + 1 < plen) {
            char n = pattern[i + 1];
            if (n == 'n') { unesc[k++] = '\n'; i++; continue; }
            if (n == 't') { unesc[k++] = '\t'; i++; continue; }
            if (n == 'r') { unesc[k++] = '\r'; i++; continue; }
        }
        unesc[k++] = pattern[i];
    }
    unesc[k] = '\0';
    int rc = _fuzzy_exact(content, unesc, match_start, match_len);
    free(unesc);
    return rc;
}

/* Strategy 6: trimmed_boundary — trim first & last pattern lines, then match a
 * line block where the block's first/last lines (trimmed) equal the pattern's. */
static int _fuzzy_trimmed_boundary(const char *content, const char *pattern,
                                   size_t *match_start, size_t *match_len)
{
    /* Split pattern into lines */
    char *pc = strdup(pattern);
    if (!pc) return 0;
    int npat = 0; char *pl[1024];
    char *s1 = NULL, *t = strtok_r(pc, "\n", &s1);
    while (t && npat < 1024) { pl[npat++] = t; t = strtok_r(NULL, "\n", &s1); }
    if (npat == 0) { free(pc); return 0; }
    /* trim first/last */
    char *f = pl[0];
    while (*f == ' ' || *f == '\t') f++;
    pl[0] = f;
    if (npat > 1) {
        char *l = pl[npat - 1];
        while (*l == ' ' || *l == '\t') l++;
        pl[npat - 1] = l;
    }
    char *cc = strdup(content);
    if (!cc) { free(pc); return 0; }
    int ncon = 0; char *cl[8192];
    char *s2 = NULL; char *ct = strtok_r(cc, "\n", &s2);
    while (ct && ncon < 8192) { cl[ncon++] = ct; ct = strtok_r(NULL, "\n", &s2); }

    int found = 0, start_line = 0;
    for (int i = 0; i + npat <= ncon; i++) {
        char *blo = cl[i];
        while (*blo == ' ' || *blo == '\t') blo++;
        char *bhi = cl[i + npat - 1];
        while (*bhi == ' ' || *bhi == '\t') bhi++;
        if (strcmp(blo, pl[0]) == 0 && strcmp(bhi, pl[npat - 1]) == 0) {
            int ok = 1;
            for (int j = 1; j < npat - 1; j++) if (strcmp(cl[i + j], pl[j]) != 0) { ok = 0; break; }
            if (ok) { found = 1; start_line = i; break; }
        }
    }
    free(pc); free(cc);
    if (!found) return 0;
    size_t sp, ep;
    _line_positions(content, start_line, start_line + npat, &sp, &ep, strlen(content));
    *match_start = sp; *match_len = ep - sp;
    return 1;
}

/* Unicode map: em/en dashes, smart quotes, ellipsis, NBSP -> ASCII.
 * Stored as UTF-8 literal (multi-byte) -> ASCII replacement, since source
 * text arrives as UTF-8, not as single wide chars. */
static const struct { const char *from; const char *to; } g_unicode_map[] = {
    {"\xe2\x80\x9c", "\""}, {"\xe2\x80\x9d", "\""},   /* “ ” */
    {"\xe2\x80\x98", "'"}, {"\xe2\x80\x99", "'"},     /* ‘ ’ */
    {"\xe2\x80\x94", "--"}, {"\xe2\x80\x93", "-"},     /* — – */
    {"\xe2\x80\xa6", "..."}, {"\xc2\xa0", " "},        /* … NBSP */
    {NULL, NULL}
};

/* Codepoint length of the UTF-8 sequence starting at s (1-4 bytes). */
static int _utf8_len(const unsigned char *s)
{
    if (s[0] < 0x80) return 1;
    if ((s[0] & 0xE0) == 0xC0) return 2;
    if ((s[0] & 0xF0) == 0xE0) return 3;
    if ((s[0] & 0xF8) == 0xF0) return 4;
    return 1;
}

/* Build a normalized copy of s with unicode mapped to ASCII.
 * Works in CODExPOINT units (matching Python's model): pos_map[] has
 * (n_codepoints+1) entries mapping each original codepoint index to its
 * normalized codepoint index. Returns malloc'd normalized string (UTF-8). */
static char *_unicode_normalize_map(const char *s, size_t **pos_map_out, size_t *n_codepoints_out)
{
    size_t n = strlen(s);
    /* worst case: every byte is a 1-byte codepoint */
    size_t *pm = (size_t *)malloc((n + 1) * sizeof(size_t));
    if (!pm) return NULL;
    size_t cap = n * 4 + 16;
    char *out = (char *)malloc(cap);
    if (!out) { free(pm); return NULL; }
    size_t ni = 0;          /* normalized codepoint index */
    size_t cpi = 0;         /* original codepoint index */
    for (size_t i = 0; i < n; ) {
        pm[cpi] = ni;
        int cl = _utf8_len((const unsigned char *)(s + i));
        const char *rep = NULL; size_t flen = 0;
        for (int k = 0; g_unicode_map[k].from; k++) {
            size_t fl = strlen(g_unicode_map[k].from);
            if ((size_t)cl == fl && strncmp(s + i, g_unicode_map[k].from, fl) == 0) {
                rep = g_unicode_map[k].to; flen = fl; break;
            }
        }
        if (rep) {
            size_t rl = strlen(rep);
            for (size_t r = 0; r < rl; r++) out[ni++] = rep[r];
        } else {
            for (int r = 0; r < cl; r++) out[ni++] = s[i + r];
        }
        i += (flen ? flen : (size_t)cl);
        cpi++;
    }
    pm[cpi] = ni;           /* sentinel: one past last codepoint */
    out[ni] = '\0';
    *pos_map_out = pm;
    if (n_codepoints_out) *n_codepoints_out = cpi;
    (void)n;
    return out;
}

/* Map normalized (codepoint start,end) back to original BYTE positions. */
static void _norm_to_orig(size_t *pm, size_t n_codepoints, size_t nstart, size_t nend,
                           const char *orig, size_t *ostart, size_t *oend)
{
    size_t *norm_to_orig = (size_t *)malloc((n_codepoints + 1) * sizeof(size_t));
    if (!norm_to_orig) { *ostart = 0; *oend = strlen(orig); return; }
    for (size_t i = 0; i <= n_codepoints; i++) norm_to_orig[i] = (size_t)-1;
    for (size_t i = 0; i <= n_codepoints; i++)
        if (norm_to_orig[pm[i]] == (size_t)-1) norm_to_orig[pm[i]] = i;
    size_t ocpi = norm_to_orig[nstart];           /* original codepoint index */
    size_t oepi = ocpi;
    while (oepi < n_codepoints && pm[oepi] < nend) oepi++;
    /* convert codepoint indices -> byte offsets */
    size_t bo = 0;
    for (size_t i = 0; i < ocpi; i++) bo += (size_t)_utf8_len((const unsigned char *)(orig + bo));
    *ostart = bo;
    size_t be = bo;
    for (size_t i = ocpi; i < oepi; i++) be += (size_t)_utf8_len((const unsigned char *)(orig + be));
    *oend = be;
    free(norm_to_orig);
}

/* Preserve Unicode characters from the file in the replacement string.
 * Mirrors Python's _preserve_unicode_in_replacement: the file region has
 * Unicode (em-dashes, smart quotes) but old/new are ASCII; writing new_string
 * verbatim would corrupt the file's Unicode. Diff old->new in normalized space
 * and apply only the actual edits, keeping the file's original text (with its
 * Unicode) for unchanged spans. Returns malloc'd string (caller frees). */
static char *_preserve_unicode_in_replacement(const char *file_region,
                                               const char *old_string,
                                               const char *new_string)
{
    size_t *opm = NULL, onp = 0;
    char *norm_old = _unicode_normalize_map(old_string, &opm, &onp);
    if (!norm_old) return strdup(new_string);
    size_t *fpm = NULL, fnp = 0;
    char *norm_file = _unicode_normalize_map(file_region, &fpm, &fnp);
    if (!norm_file) { free(norm_old); free(opm); return strdup(new_string); }

    if (strcmp(norm_old, norm_file) != 0) { /* sanity: should match */
        free(norm_old); free(opm); free(norm_file); free(fpm);
        return strdup(new_string);
    }

    /* fpm is codepoint-indexed: fpm[i] = normalized codepoint index of the
     * i-th file_region codepoint. We use it directly below to walk the file
     * region during reconstruction (contraction-safe). */

    /* LCS over normalized old vs new_string -> opcodes. */
    int la = (int)strlen(norm_old), lb = (int)strlen(new_string);
    int *dp = (int *)calloc((size_t)(la + 1) * (size_t)(lb + 1), sizeof(int));
    if (!dp) { free(norm_old); free(opm); free(norm_file); free(fpm); return strdup(new_string); }
    for (int i = 1; i <= la; i++)
        for (int j = 1; j <= lb; j++)
            dp[i * (lb + 1) + j] = (norm_old[i-1] == new_string[j-1])
                ? dp[(i-1)*(lb+1)+j-1] + 1
                : (dp[(i-1)*(lb+1)+j] > dp[i*(lb+1)+j-1] ? dp[(i-1)*(lb+1)+j] : dp[i*(lb+1)+j-1]);

    size_t cap = strlen(new_string) + strlen(file_region) + 16;
    char *out = (char *)malloc(cap);
    if (!out) { free(dp); free(norm_old); free(opm); free(norm_file); free(fpm); return strdup(new_string); }
    out[0] = '\0';

    int i = la, j = lb;
    typedef struct { int tag, i1, i2, j1, j2; } op_t;
    op_t ops[4096]; int nops = 0;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && norm_old[i-1] == new_string[j-1]) {
            ops[nops++] = (op_t){0, i-1, i, j-1, j}; i--; j--;
        } else if (j > 0 && (i == 0 || dp[i*(lb+1)+j-1] >= dp[(i-1)*(lb+1)+j])) {
            ops[nops++] = (op_t){2, i, i, j-1, j}; j--;          /* insert */
        } else {
            ops[nops++] = (op_t){1, i-1, i, j, j}; i--;          /* delete */
        }
    }
    /* Reconstruct the output. For "equal" opcodes (unchanged spans) copy the
     * file region's ORIGINAL bytes; for insert/delete use new_string. The op
     * indices are in normalized space, so we walk the file region by its own
     * codepoints using the forward fpm map (codepoint i's normalized index is
     * fpm[i]). This is inherently contraction-safe: a file codepoint whose
     * normalized index falls outside [i1,i2) is simply skipped, so an em-dash
     * (1 file cp -> 2 normalized cps "--") is copied as one unit when its
     * normalized index range is wholly inside [i1,i2). */
    for (int k = nops - 1; k >= 0; k--) {
        op_t o = ops[k];
        if (o.tag == 0) { /* equal: keep file region's ORIGINAL text */
            size_t bo = 0;   /* byte offset of current file codepoint */
            size_t cp = 0;   /* file codepoint index */
            /* advance to the first file cp whose normalized index >= i1 */
            while (cp < fnp && fpm[cp] < (size_t)o.i1) {
                bo += (size_t)_utf8_len((const unsigned char *)(file_region + bo));
                cp++;
            }
            size_t eo = bo;
            /* include file cps whose normalized index is in [i1, i2) */
            while (cp < fnp && fpm[cp] < (size_t)o.i2) {
                eo += (size_t)_utf8_len((const unsigned char *)(file_region + eo));
                cp++;
            }
            size_t len = eo - bo;
            if (len > cap - strlen(out) - 1) len = cap - strlen(out) - 1;
            if (len) strncat(out, file_region + bo, len);
        } else if (o.tag == 1) { /* delete: skip (already absent) */
        } else { /* insert: use new_string span */
            size_t len = o.j2 - o.j1;
            if (len > cap - strlen(out) - 1) len = cap - strlen(out) - 1;
            strncat(out, new_string + o.j1, len);
        }
    }
    free(dp); free(norm_old); free(opm); free(norm_file); free(fpm); return out;
    return out;
}

/* Strategy 7: unicode_normalized. */
static int _fuzzy_unicode_normalized(const char *content, const char *pattern,
                                     size_t *match_start, size_t *match_len)
{
    size_t *cpm = NULL, *ppm = NULL, cnp = 0, pnp = 0;
    char *nc = _unicode_normalize_map(content, &cpm, &cnp);
    char *np = _unicode_normalize_map(pattern, &ppm, &pnp);
    if (!nc || !np) { free(nc); free(np); free(cpm); free(ppm); return 0; }
    int rc = 0;
    if (strcmp(nc, content) != 0 || strcmp(np, pattern) != 0) {
        size_t ms = 0, ml = 0;
        if (_fuzzy_exact(nc, np, &ms, &ml) || _fuzzy_line_trimmed(nc, np, &ms, &ml)) {
            size_t os, oe;
            _norm_to_orig(cpm, cnp, ms, ms + ml, content, &os, &oe);
            *match_start = os; *match_len = oe - os;
            rc = 1;
        }
    }
    free(nc); free(np); free(cpm); free(ppm);
    return rc;
}

/* Strategy 8: block_anchor — match first/last pattern lines (unicode-trimmed),
 * accept when middle similarity >= 0.50 (single candidate) or 0.70 (multiple). */
static int _fuzzy_block_anchor(const char *content, const char *pattern,
                               size_t *match_start, size_t *match_len)
{
    /* Use normalized content/pattern for line comparison; original for offsets. */
    size_t *cpm = NULL, *ppm = NULL;
    char *norm_c = _unicode_normalize_map(content, &cpm, NULL);
    char *norm_p = _unicode_normalize_map(pattern, &ppm, NULL);
    if (!norm_c || !norm_p) { free(norm_c); free(norm_p); free(cpm); free(ppm); return 0; }
    int npat = 0; char *pl[1024];
    char *s1 = NULL, *t = strtok_r(norm_p, "\n", &s1);
    while (t && npat < 1024) { pl[npat++] = t; t = strtok_r(NULL, "\n", &s1); }
    if (npat < 2) { free(norm_c); free(norm_p); free(cpm); free(ppm); return 0; }
    char *f = pl[0]; while (*f == ' ' || *f == '\t') f++;
    char *l = pl[npat - 1]; while (*l == ' ' || *l == '\t') l++;

    int ncon = 0; char *cl[8192];
    char *s2 = NULL, *ct = strtok_r(norm_c, "\n", &s2);
    while (ct && ncon < 8192) { cl[ncon++] = ct; ct = strtok_r(NULL, "\n", &s2); }

    int candidates[8192], ncand = 0;
    for (int i = 0; i + npat <= ncon; i++) {
        char *blo = cl[i]; while (*blo == ' ' || *blo == '\t') blo++;
        char *bhi = cl[i + npat - 1]; while (*bhi == ' ' || *bhi == '\t') bhi++;
        if (strcmp(blo, f) == 0 && strcmp(bhi, l) == 0) candidates[ncand++] = i;
    }
    double threshold = (ncand == 1) ? 0.50 : 0.70;
    int found = 0, start_line = 0;
    for (int c = 0; c < ncand; c++) {
        int i = candidates[c];
        double sim = 1.0;
        if (npat > 2) {
            char *cmid[1024]; int m = 0;
            for (int j = 1; j < npat - 1; j++) cmid[m++] = cl[i + j];
            char *pmid[1024]; int pm = 0;
            for (int j = 1; j < npat - 1; j++) pmid[pm++] = pl[j];
            /* join */
            char cb[8192] = {0}, pb[8192] = {0};
            for (int j = 0; j < m; j++) { strncat(cb, cmid[j], sizeof(cb) - strlen(cb) - 1); strncat(cb, "\n", 1); }
            for (int j = 0; j < pm; j++) { strncat(pb, pmid[j], sizeof(pb) - strlen(pb) - 1); strncat(pb, "\n", 1); }
            sim = _line_sim(cb, pb);
        }
        if (sim >= threshold) { found = 1; start_line = i; break; }
    }
    free(norm_c); free(norm_p); free(cpm); free(ppm);
    if (!found) return 0;
    size_t sp, ep;
    _line_positions(content, start_line, start_line + npat, &sp, &ep, strlen(content));
    *match_start = sp; *match_len = ep - sp;
    return 1;
}

/* Strategy 9: context_aware — block where >= 50% of lines have line-sim >= 0.80. */
static int _fuzzy_context_aware(const char *content, const char *pattern,
                                size_t *match_start, size_t *match_len)
{
    int npat = 0; char *pl[1024];
    char *pc = strdup(pattern);
    if (!pc) return 0;
    char *s1 = NULL, *t = strtok_r(pc, "\n", &s1);
    while (t && npat < 1024) { pl[npat++] = t; t = strtok_r(NULL, "\n", &s1); }
    if (npat == 0) { free(pc); return 0; }
    int ncon = 0; char *cl[8192];
    char *cc = strdup(content);
    if (!cc) { free(pc); return 0; }
    char *s2 = NULL, *ct = strtok_r(cc, "\n", &s2);
    while (ct && ncon < 8192) { cl[ncon++] = ct; ct = strtok_r(NULL, "\n", &s2); }

    int found = 0, start_line = 0;
    for (int i = 0; i + npat <= ncon; i++) {
        int high = 0;
        for (int j = 0; j < npat; j++) {
            char *a = pl[j], *b = cl[i + j];
            while (*a == ' ' || *a == '\t') a++;
            while (*b == ' ' || *b == '\t') b++;
            if (_line_sim(a, b) >= 0.80) high++;
        }
        if (high >= npat * 0.5) { found = 1; start_line = i; break; }
    }
    free(pc); free(cc);
    if (!found) return 0;
    size_t sp, ep;
    _line_positions(content, start_line, start_line + npat, &sp, &ep, strlen(content));
    *match_start = sp; *match_len = ep - sp;
    return 1;
}

/* Escape-drift guard (mirrors _detect_escape_drift): if new_string carries a
 * spurious \' or \" that was copy-pasted as context but is absent from the
 * matched file region, refuse the edit. Returns 1 if drift detected (sets
 * *err), 0 otherwise. */
static int _detect_escape_drift(const char *content, size_t ms, size_t ml,
                                const char *old_string, const char *new_string, char *err, size_t errsz)
{
    if (!strstr(new_string, "\\'") && !strstr(new_string, "\\\"")) return 0;
    char *region = (char *)malloc(ml + 1);
    if (!region) return 0;
    memcpy(region, content + ms, ml); region[ml] = '\0';
    int drift = 0;
    for (const char *suspect = "\\'"; ; suspect = "\\\"") {
        if (strstr(new_string, suspect) && strstr(old_string, suspect) && !strstr(region, suspect))
            drift = 1;
        if (suspect == "\\\"") break;
    }
    free(region);
    if (drift) {
        snprintf(err, errsz, "Escape-drift detected: old_string and new_string contain the "
                 "literal sequence \\' or \\\" but the matched region of the file does not. "
                 "Re-read the file and pass old_string/new_string without backslash-escaping.");
        return 1;
    }
    return 0;
}

/* Re-indent new_string to the file region's actual base indent (mirrors
 * _reindent_replacement). Returns malloc'd adjusted string. */
static char *_reindent_replacement(const char *file_region, const char *old_string, const char *new_string)
{
    /* leading whitespace length of first meaningful line of old and file.
     * Mirrors Python's _leading_whitespace(_first_meaningful_line(...)). */
    const char *obs = old_string, *fbs = file_region;
    while (*obs == ' ' || *obs == '\t' || *obs == '\n' || *obs == '\r') obs++;
    while (*fbs == ' ' || *fbs == '\t' || *fbs == '\n' || *fbs == '\r') fbs++;
    size_t oi = (size_t)(obs - old_string);   /* leading ws length of old */
    size_t fi = (size_t)(fbs - file_region);  /* leading ws length of file */
    if (oi == fi) return strdup(new_string);
    char old_ind[256] = {0}, file_ind[256] = {0};
    memcpy(old_ind, old_string, oi); old_ind[oi] = '\0';
    memcpy(file_ind, file_region, fi); file_ind[fi] = '\0';
    size_t nlen = strlen(new_string) * 2 + 256;
    char *out = (char *)malloc(nlen);
    if (!out) return strdup(new_string);
    out[0] = '\0';
    const char *p = new_string;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        char line[4096]; memcpy(line, p, ll); line[ll] = '\0';
        if (line[0] && !(line[0] == ' ' || line[0] == '\t')) {
            strncat(out, file_ind, nlen - strlen(out) - 1);
            strncat(out, line, nlen - strlen(out) - 1);
        } else if (strncmp(line, old_ind, oi) == 0) {
            strncat(out, file_ind, nlen - strlen(out) - 1);
            strncat(out, line + oi, nlen - strlen(out) - 1);
        } else {
            strncat(out, line, nlen - strlen(out) - 1);
        }
        if (nl) { strncat(out, "\n", nlen - strlen(out) - 1); p = nl + 1; }
        else break;
    }
    return out;
}

/* Fuzzy matching chain — tries strategies in order, returns 1 if any matched.
 * Now mirrors Python's 9-strategy chain (exact, line_trimmed,
 * whitespace_normalized, indentation_flexible, escape_normalized,
 * trimmed_boundary, unicode_normalized, block_anchor, context_aware). */
static int _fuzzy_find(const char *content, const char *pattern,
                       size_t *match_start, size_t *match_len, const char **strategy_out)
{
    typedef int (*fuzzy_fn)(const char *, const char *, size_t *, size_t *);
    typedef struct { const char *name; fuzzy_fn fn; } strategy_entry;

    strategy_entry chain[] = {
        {"exact", _fuzzy_exact},
        {"line_trimmed", _fuzzy_line_trimmed},
        {"indentation_flexible", _fuzzy_indentation_flexible},
        {"whitespace_normalized", _fuzzy_whitespace_normalized},
        {"escape_normalized", _fuzzy_escape_normalized},
        {"trimmed_boundary", _fuzzy_trimmed_boundary},
        {"unicode_normalized", _fuzzy_unicode_normalized},
        {"block_anchor", _fuzzy_block_anchor},
        {"context_aware", _fuzzy_context_aware},
    };
    int n = sizeof(chain) / sizeof(chain[0]);

    for (int i = 0; i < n; i++) {
        if (chain[i].fn(content, pattern, match_start, match_len)) {
            *strategy_out = chain[i].name;
            return 1;
        }
    }
    return 0;
}

/* ================================================================
 *  V4A Patch Format Parser
 * ================================================================
 *
 * V4A format:
 *   *** Begin Patch
 *   *** Update File: path/to/file
 *   @@ context hint @@
 *    context line (space prefix)
 *   -removed line (minus prefix)
 *   +added line (plus prefix)
 *   *** Add File: path/to/new
 *   +new content
 *   *** Delete File: path/to/old
 *   *** End Patch
 */

/* Operation types */
#define V4A_UPDATE 0
#define V4A_ADD    1
#define V4A_DELETE 2

/* A single hunk line */
typedef struct {
    char prefix;   /* ' ', '-', '+' */
    char *content;
} v4a_hunk_line_t;

/* A hunk (group of changes in a file) */
typedef struct {
    char *context_hint;
    v4a_hunk_line_t *lines;
    int nlines;
} v4a_hunk_t;

/* A single V4A operation */
typedef struct {
    int type;         /* V4A_UPDATE, V4A_ADD, V4A_DELETE */
    char *file_path;
    v4a_hunk_t *hunks;
    int nhunks;
} v4a_operation_t;

/* Forward declaration */
static void finalize_op(v4a_operation_t *op, v4a_hunk_t *hunk,
                         bool *in_hunk, v4a_operation_t **ops, int *nops);

/* Port of Python tools/patch_parser.py:parse_v4a_patch(). */
/* Parse a V4A patch and return operations list. */
static int parse_v4a_patch(const char *patch_content, v4a_operation_t **ops_out, int *nops_out, char **err_out)
{
    v4a_operation_t *ops = NULL;
    int nops = 0;
    v4a_operation_t cur_op;
    v4a_hunk_t cur_hunk;
    bool in_op = false, in_hunk = false;

    char *buf = strdup(patch_content);
    if (!buf) { *err_out = strdup("OOM"); return -1; }

    /* Find Begin/End markers */
    char *begin = strstr(buf, "*** Begin Patch");
    if (!begin) begin = strstr(buf, "***Begin Patch");
    char *end = strstr(buf, "*** End Patch");
    if (!end) end = strstr(buf, "***End Patch");

    if (!begin) begin = buf;
    if (!end) end = buf + strlen(buf);

    /* Skip past begin marker line */
    char *body = begin;
    if (begin > buf) {
        char *nl = strchr(begin, '\n');
        if (nl) body = nl + 1;
    }

    /* Split body into lines */
    char **lines = NULL;
    int nlines = 0;
    {
        size_t body_len = (size_t)(end - body);
        char *work = malloc(body_len + 1);
        if (!work) { free(buf); *err_out = strdup("OOM"); return -1; }
        memcpy(work, body, body_len);
        work[body_len] = '\0';

        char *save = NULL;
        char *tok = strtok_r(work, "\n", &save);
        while (tok) {
            char **tmp = realloc(lines, (nlines + 1) * sizeof(char *));
            if (!tmp) { free(work); free(buf); *err_out = strdup("OOM"); return -1; }
            lines = tmp;
            lines[nlines++] = strdup(tok);
            tok = strtok_r(NULL, "\n", &save);
        }
        free(work);
    }

    memset(&cur_op, 0, sizeof(cur_op));
    memset(&cur_hunk, 0, sizeof(cur_hunk));

    for (int i = 0; i < nlines; i++) {
        const char *line = lines[i];

        /* Check for file operation markers */
        if (strncmp(line, "*** Update File:", 16) == 0) {
            if (in_op) { finalize_op(&cur_op, &cur_hunk, &in_hunk, &ops, &nops); }
            cur_op.type = V4A_UPDATE;
            cur_op.file_path = strdup(line + 16);
            while (*cur_op.file_path == ' ') {
                memmove(cur_op.file_path, cur_op.file_path + 1, strlen(cur_op.file_path));
            }
            in_op = true;

        } else if (strncmp(line, "*** Add File:", 13) == 0) {
            if (in_op) { finalize_op(&cur_op, &cur_hunk, &in_hunk, &ops, &nops); }
            cur_op.type = V4A_ADD;
            cur_op.file_path = strdup(line + 13);
            while (*cur_op.file_path == ' ') {
                memmove(cur_op.file_path, cur_op.file_path + 1, strlen(cur_op.file_path));
            }
            in_op = true;

        } else if (strncmp(line, "*** Delete File:", 16) == 0) {
            if (in_op) { finalize_op(&cur_op, &cur_hunk, &in_hunk, &ops, &nops); }
            cur_op.type = V4A_DELETE;
            cur_op.file_path = strdup(line + 16);
            while (*cur_op.file_path == ' ') {
                memmove(cur_op.file_path, cur_op.file_path + 1, strlen(cur_op.file_path));
            }
            /* DELETE — save immediately, no hunks */
            v4a_operation_t *tmp = realloc(ops, (nops + 1) * sizeof(v4a_operation_t));
            if (tmp) { ops = tmp; ops[nops++] = cur_op; }
            memset(&cur_op, 0, sizeof(cur_op));
            in_op = false;

        } else if (line[0] == '@' && line[1] == '@') {
            if (in_op && in_hunk && cur_hunk.nlines > 0) {
                v4a_hunk_t *tmp = realloc(cur_op.hunks, (cur_op.nhunks + 1) * sizeof(v4a_hunk_t));
                if (tmp) { cur_op.hunks = tmp; cur_op.hunks[cur_op.nhunks++] = cur_hunk; }
                memset(&cur_hunk, 0, sizeof(cur_hunk));
            }
            const char *hs = line + 2;
            const char *he = strstr(hs, "@@");
            if (he) {
                size_t hlen = (size_t)(he - hs);
                cur_hunk.context_hint = malloc(hlen + 1);
                if (cur_hunk.context_hint) {
                    memcpy(cur_hunk.context_hint, hs, hlen);
                    cur_hunk.context_hint[hlen] = '\0';
                }
            }
            in_hunk = true;

        } else if (in_op && line[0] != '\0') {
            if (!in_hunk) {
                memset(&cur_hunk, 0, sizeof(cur_hunk));
                in_hunk = true;
            }
            char prefix = ' ';
            const char *content = line;
            if (line[0] == '+' || line[0] == '-' || line[0] == ' ') {
                prefix = line[0];
                content = line + 1;
            }
            v4a_hunk_line_t *tmp = realloc(cur_hunk.lines, (cur_hunk.nlines + 1) * sizeof(v4a_hunk_line_t));
            if (tmp) {
                cur_hunk.lines = tmp;
                cur_hunk.lines[cur_hunk.nlines].prefix = prefix;
                cur_hunk.lines[cur_hunk.nlines].content = strdup(content);
                cur_hunk.nlines++;
            }
        }
    }

    /* Save last op */
    if (in_op) {
        if (in_hunk && cur_hunk.nlines > 0) {
            v4a_hunk_t *tmp = realloc(cur_op.hunks, (cur_op.nhunks + 1) * sizeof(v4a_hunk_t));
            if (tmp) { cur_op.hunks = tmp; cur_op.hunks[cur_op.nhunks++] = cur_hunk; }
        }
        v4a_operation_t *tmp = realloc(ops, (nops + 1) * sizeof(v4a_operation_t));
        if (tmp) { ops = tmp; ops[nops++] = cur_op; }
    }

    for (int i = 0; i < nlines; i++) free(lines[i]);
    free(lines);
    free(buf);

    *ops_out = ops;
    *nops_out = nops;
    *err_out = NULL;
    return 0;
}

/* Helper: finalize current operation and append to list */
static void finalize_op(v4a_operation_t *op, v4a_hunk_t *hunk,
                         bool *in_hunk, v4a_operation_t **ops, int *nops)
{
    if (*in_hunk && hunk->nlines > 0) {
        v4a_hunk_t *tmp = realloc(op->hunks, (op->nhunks + 1) * sizeof(v4a_hunk_t));
        if (tmp) { op->hunks = tmp; op->hunks[op->nhunks++] = *hunk; }
        memset(hunk, 0, sizeof(*hunk));
        *in_hunk = false;
    }
    v4a_operation_t *tmp = realloc(*ops, (*nops + 1) * sizeof(v4a_operation_t));
    if (tmp) { *ops = tmp; (*ops)[*nops] = *op; (*nops)++; }
    memset(op, 0, sizeof(*op));
}

/* Apply a V4A UPDATE hunk: search+replace in file buffer */
static char *apply_v4a_hunk(const char *file_content, v4a_hunk_t *hunk,
                             long *file_size_out, char **error_out)
{
    /* Build search string: ' ' or '-' prefix lines */
    size_t search_len = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '-') {
            search_len += strlen(hunk->lines[i].content) + 1;
        }
    }
    if (search_len == 0) search_len = 1;
    char *search_str = calloc(search_len + 1, 1);
    if (!search_str) { *error_out = strdup("OOM"); return NULL; }
    size_t pos = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '-') {
            if (pos > 0) search_str[pos++] = '\n';
            size_t clen = strlen(hunk->lines[i].content);
            memcpy(search_str + pos, hunk->lines[i].content, clen);
            pos += clen;
        }
    }
    if (pos > 0 && search_str[pos-1] == '\n') search_str[--pos] = '\0';
    search_str[pos] = '\0';
    search_len = pos; /* use actual built length, not pre-computed */

    /* Build replacement string: ' ' or '+' prefix lines */
    size_t repl_len = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '+') {
            repl_len += strlen(hunk->lines[i].content) + 1;
        }
    }
    if (repl_len == 0) repl_len = 1;
    char *repl_str = calloc(repl_len + 1, 1);
    if (!repl_str) { free(search_str); *error_out = strdup("OOM"); return NULL; }
    pos = 0;
    for (int i = 0; i < hunk->nlines; i++) {
        if (hunk->lines[i].prefix == ' ' || hunk->lines[i].prefix == '+') {
            if (pos > 0) repl_str[pos++] = '\n';
            size_t clen = strlen(hunk->lines[i].content);
            memcpy(repl_str + pos, hunk->lines[i].content, clen);
            pos += clen;
        }
    }
    if (pos > 0 && repl_str[pos-1] == '\n') repl_str[--pos] = '\0';
    repl_str[pos] = '\0';
    repl_len = pos; /* use actual built length, not pre-computed */

    /* Find search string in file content */
    const char *match = strstr(file_content, search_str);
    size_t offset, match_len;
    bool fuzzy = false;

    if (!match) {
        /* Try all 4 fuzzy strategies */
        size_t ms, ml;
        const char *best_strategy = NULL;
        if (!_fuzzy_find(file_content, search_str, &ms, &ml, &best_strategy)) {
            size_t content_len = strlen(file_content);
            *error_out = malloc(512);
            if (*error_out) {
                snprintf(*error_out, 512,
                    "Hunk not found (tried exact, line-trimmed, indentation-flexible, "
                    "whitespace-normalized). "
                    "Snippet around closest context match near offset %zu:\n%.*s[...]",
                    content_len > 200 ? (size_t)100 : (size_t)0,
                    content_len > 200 ? 200 : (int)content_len,
                    content_len > 200 ? file_content + 100 : file_content);
            }
            free(search_str);
            free(repl_str);
            return NULL;
        }
        offset = ms;
        match_len = ml;
        fuzzy = true;
        fprintf(stderr, "[patch] Fuzzy match: strategy=%s offset=%zu\n",
                best_strategy, offset);
    } else {
        offset = (size_t)(match - file_content);
        match_len = strlen(search_str);
    }

    long fsize = (long)strlen(file_content);
    size_t newsize = fsize - match_len + repl_len + 1;
    char *result = malloc(newsize);
    if (!result) { free(search_str); free(repl_str); *error_out = strdup("OOM"); return NULL; }
    memcpy(result, file_content, offset);
    memcpy(result + offset, repl_str, repl_len);
    memcpy(result + offset + repl_len, file_content + offset + match_len, fsize - offset - match_len);
    result[newsize - 1] = '\0';
    *file_size_out = (long)(newsize - 1);

    free(search_str);
    free(repl_str);
    (void)fuzzy; /* could report in result */
    return result;
}

/* Free V4A operations */
static void free_v4a_operations(v4a_operation_t *ops, int nops)
{
    for (int i = 0; i < nops; i++) {
        free(ops[i].file_path);
        for (int j = 0; j < ops[i].nhunks; j++) {
            free(ops[i].hunks[j].context_hint);
            for (int k = 0; k < ops[i].hunks[j].nlines; k++) {
                free(ops[i].hunks[j].lines[k].content);
            }
            free(ops[i].hunks[j].lines);
        }
        free(ops[i].hunks);
    }
    free(ops);
}

/* Apply a V4A patch — main entry point (now public; reused by file_operations.patch_v4a) */
char *patch_apply_v4a(const char *patch_content, bool dry_run)
{
    v4a_operation_t *ops = NULL;
    int nops = 0;
    char *err = NULL;
    (void)dry_run;  /* Used for V4A write gating but simpler to skip for now */

    if (parse_v4a_patch(patch_content, &ops, &nops, &err) != 0 || err) {
        char *out = malloc(512);
        if (out) snprintf(out, 512, "{\"error\":\"Failed to parse V4A patch: %s\"}", err ? err : "unknown");
        free(err);
        return out;
    }

    json_node_t *results = json_new_array();
    int total_changes = 0;
    bool any_error = false;

    for (int i = 0; i < nops; i++) {
        json_node_t *op_result = json_new_object();
        json_object_set(op_result, "file", json_new_string(ops[i].file_path ? ops[i].file_path : "?"));

        switch (ops[i].type) {
            case V4A_UPDATE: {
                FILE *f = fopen(ops[i].file_path, "rb");
                if (!f) {
                    json_object_set(op_result, "error", json_new_string("Cannot open file for reading"));
                    any_error = true; break;
                }
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                char *content = malloc((size_t)fsize + 1);
                if (!content) { fclose(f); json_object_set(op_result, "error", json_new_string("OOM")); any_error = true; break; }
                size_t br = fread(content, 1, (size_t)fsize, f);
                fclose(f);
                content[br] = '\0';

                char *current = content;
                long current_size = (long)br;
                int hunks_applied = 0;
                bool hunk_error = false;

                for (int h = 0; h < ops[i].nhunks; h++) {
                    char *hunk_err = NULL;
                    long new_size = 0;
                    char *new_content = apply_v4a_hunk(current, &ops[i].hunks[h], &new_size, &hunk_err);
                    if (!new_content) {
                        json_object_set(op_result, "error", json_new_string(hunk_err ? hunk_err : "Hunk apply failed"));
                        free(hunk_err);
                        hunk_error = true;
                        break;
                    }
                    free(current);
                    current = new_content;
                    current_size = new_size;
                    hunks_applied++;
                    free(hunk_err);
                }

                if (!hunk_error && current) {
                    f = fopen(ops[i].file_path, "w");
                    if (!f) {
                        json_object_set(op_result, "error", json_new_string("Cannot open file for writing"));
                        any_error = true;
                    } else {
                        fwrite(current, 1, (size_t)current_size, f);
                        fclose(f);
                        json_object_set(op_result, "success", json_new_bool(true));
                        json_object_set(op_result, "hunks_applied", json_new_number((double)hunks_applied));
                        total_changes++;
                    }
                }
                free(current);
                break;
            }

            case V4A_ADD: {
                size_t add_len = 0;
                for (int h = 0; h < ops[i].nhunks; h++) {
                    for (int k = 0; k < ops[i].hunks[h].nlines; k++) {
                        if (ops[i].hunks[h].lines[k].prefix == '+')
                            add_len += strlen(ops[i].hunks[h].lines[k].content) + 1;
                    }
                }
                if (add_len > 0) add_len--; /* remove trailing \n count */

                char *add_content = calloc(add_len + 1, 1);
                if (!add_content) {
                    json_object_set(op_result, "error", json_new_string("OOM"));
                    any_error = true; break;
                }
                size_t ap = 0;
                for (int h = 0; h < ops[i].nhunks; h++) {
                    for (int k = 0; k < ops[i].hunks[h].nlines; k++) {
                        if (ops[i].hunks[h].lines[k].prefix == '+') {
                            size_t clen = strlen(ops[i].hunks[h].lines[k].content);
                            if (ap > 0) add_content[ap++] = '\n';
                            memcpy(add_content + ap, ops[i].hunks[h].lines[k].content, clen);
                            ap += clen;
                        }
                    }
                }

                char dir_copy[4096];
                snprintf(dir_copy, sizeof(dir_copy), "%s", ops[i].file_path);
                char *slash = strrchr(dir_copy, '/');
                if (slash) { *slash = '\0'; mkdir(dir_copy, 0755); }

                FILE *f = fopen(ops[i].file_path, "w");
                if (!f) {
                    json_object_set(op_result, "error", json_new_string("Cannot open file for writing"));
                    any_error = true; free(add_content); break;
                }
                if (add_len > 0) fwrite(add_content, 1, add_len, f);
                fclose(f);
                json_object_set(op_result, "success", json_new_bool(true));
                json_object_set(op_result, "bytes_written", json_new_number((double)add_len));
                total_changes++;
                free(add_content);
                break;
            }

            case V4A_DELETE: {
                if (remove(ops[i].file_path) == 0) {
                    json_object_set(op_result, "success", json_new_bool(true));
                    json_object_set(op_result, "deleted", json_new_bool(true));
                    total_changes++;
                } else {
                    json_object_set(op_result, "error", json_new_string("Cannot delete file"));
                    any_error = true;
                }
                break;
            }
        }
        json_append(results, op_result);
    }

    json_node_t *root = json_new_object();
    json_object_set(root, "mode", json_new_string("patch"));
    json_object_set(root, "results", results);
    json_object_set(root, "operations", json_new_number((double)nops));
    json_object_set(root, "total_changes", json_new_number((double)total_changes));
    if (any_error) json_object_set(root, "partial", json_new_bool(true));
    else json_object_set(root, "success", json_new_bool(true));

    char *json_out = json_serialize(root);
    json_free(root);
    free_v4a_operations(ops, nops);
    return json_out;
}

/* ================================================================
 *  Core patch logic
 * ================================================================ */

static char *apply_patch(const char *path, const char *old_str,
                          const char *new_str, bool replace_all, bool dry_run)
{
    if (!path || !old_str) return strdup("{\"error\":\"Missing path or old_string\"}");

    /* Read entire file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        return strdup("{\"error\":\"Cannot open file for reading\"}");
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize > 10 * 1024 * 1024) { /* > 10MB */
        fclose(f);
        return strdup("{\"error\":\"File too large (>10MB)\"}");
    }
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return strdup("{\"error\":\"OOM\"}"); }
    size_t bytes_read = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[bytes_read] = '\0';

    size_t old_len = strlen(old_str);
    if (old_len == 0) {
        free(content);
        return strdup("{\"error\":\"old_string cannot be empty\"}");
    }

    /* Count occurrences using exact match */
    int count = 0;
    const char *p = content;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }

    /* Strategy used for matching (reported in output) */
    const char *strategy_used = "exact";
    size_t match_offset = 0, match_length = old_len;

    if (count == 0) {
        /* Try fuzzy matching strategies */
        if (_fuzzy_find(content, old_str, &match_offset, &match_length, &strategy_used)) {
            count = 1;
            /* Update old_len to match_length for correct result_size calculation */
            old_len = match_length;

            /* Escape-drift guard (mirrors Python _detect_escape_drift): refuse the
             * edit if new_str carries a spurious \' or \" absent from the file region. */
            char drift_err[512];
            if (_detect_escape_drift(content, match_offset, match_length, old_str,
                                    new_str ? new_str : "", drift_err, sizeof(drift_err))) {
                free(content);
                char *out = malloc(512 + strlen(drift_err));
                if (out) snprintf(out, 512 + strlen(drift_err), "{\"error\":\"%s\"}", drift_err);
                else out = strdup("{\"error\":\"escape-drift detected\"}");
                return out;
            }
        } else {
            free(content);
            return strdup("{\"error\":\"old_string not found in file (tried exact + 9 fuzzy strategies)\"}");
        }
    }

    if (!replace_all && count > 1) {
        /* Conflict resolution: build JSON with snippets for each match */
        json_node_t *result = json_new_object();
        json_object_set(result, "conflict", json_new_bool(true));
        json_object_set(result, "error", json_new_string("old_string found multiple times in file — conflict resolution needed"));
        json_object_set(result, "count", json_new_number((double)count));

        json_node_t *matches = json_new_array();
        const char *scan = content;
        size_t search_len = strlen(old_str);
        for (int i = 0; i < count && i < 20; i++) {
            const char *found = strstr(scan, old_str);
            if (!found) break;
            size_t offset = (size_t)(found - content);

            json_node_t *m = json_new_object();
            json_object_set(m, "offset", json_new_number((double)offset));

            /* Build context snippet: 40 chars before, match, 40 chars after */
            const char *ctx_start = found > content + 40 ? found - 40 : content;
            size_t ctx_prefix_len = (size_t)(found - ctx_start);
            char snippet[512];
            size_t sn = 0;
            if (ctx_prefix_len > 0) {
                sn += snprintf(snippet + sn, sizeof(snippet) - sn, "%.*s", (int)ctx_prefix_len, ctx_start);
            }
            sn += snprintf(snippet + sn, sizeof(snippet) - sn, "[MATCH]");
            size_t remaining = sizeof(snippet) - sn - 1;
            size_t after_len = strlen(found);
            if (after_len > remaining) after_len = remaining;
            memcpy(snippet + sn, found, after_len);
            snippet[sn + after_len] = '\0';
            json_object_set(m, "snippet", json_new_string(snippet));

            json_array_append(matches, m);
            scan = found + search_len;
        }
        json_object_set(result, "matches", matches);
        char *json_out = json_serialize(result);
        json_free(result);
        free(content);
        return json_out;
    }

    /* Conditional unescape of \\t/\\r in new_string — mirrors Python's
     * _maybe_unescape_new_string().  LLMs frequently send the two-character
     * sequences \\t and \\r inside JSON tool-call arguments where the file
     * has real tab / carriage-return bytes.  Only convert when the matched
     * file region actually contains the corresponding control byte, so that
     * legitimate writes of the literal two-character string (e.g. patching
     * Python source ``sep = "\\t"``) pass through untouched.
     *
     * \\n is intentionally excluded: newlines serialize correctly through
     * JSON and rewriting them would mangle escape sequences in string
     * literals far more often than help.
     *
     * This block determines the first matched region, then applies the
     * conditional unescape to new_str.  For replace_all the same effective
     * string is used for every occurrence.
     */
    const char *effective_new_str = new_str ? new_str : "";
    char *unescaped_new_str = NULL;
    if (effective_new_str[0] &&
        (strstr(effective_new_str, "\\t") || strstr(effective_new_str, "\\r")))
    {
        /* Find the first matched region in content */
        size_t first_mstart = 0, first_mlen = 0;
        if (count == 1 && strcmp(strategy_used, "exact") != 0) {
            /* Fuzzy — match_offset/match_length already set */
            first_mstart = match_offset;
            first_mlen = match_length;
        } else {
            /* Exact — find first occurrence */
            const char *first = strstr(content, old_str);
            if (first) {
                first_mstart = (size_t)(first - content);
                first_mlen = old_len;
            }
        }

        if (first_mlen > 0) {
            /* Extract matched region and check for control bytes */
            const char *region_start = content + first_mstart;
            size_t region_len = first_mlen;
            bool has_tab = false, has_cr = false;
            for (size_t i = 0; i < region_len; i++) {
                if (region_start[i] == '\t') has_tab = true;
                if (region_start[i] == '\r') has_cr = true;
            }

            unescaped_new_str = strdup(effective_new_str);
            if (unescaped_new_str) {
                if (has_tab) {
                    /* Replace literal two-char \\t with real tab byte */
                    char *tmp = unescaped_new_str;
                    size_t tmp_len = strlen(tmp) + 256;
                    char *buf = calloc(tmp_len + 1, 1);
                    if (buf) {
                        const char *sr = tmp;
                        size_t wp = 0;
                        while (*sr) {
                            if (sr[0] == '\\' && sr[1] == 't') {
                                buf[wp++] = '\t';
                                sr += 2;
                            } else {
                                buf[wp++] = *sr++;
                            }
                            if (wp + 4 >= tmp_len) {
                                tmp_len += 256;
                                char *nb = realloc(buf, tmp_len + 1);
                                if (!nb) break;
                                buf = nb;
                            }
                        }
                        buf[wp] = '\0';
                        free(unescaped_new_str);
                        unescaped_new_str = buf;
                    }
                }
                if (has_cr && unescaped_new_str) {
                    /* Replace literal \\r with real CR byte */
                    char *tmp = unescaped_new_str;
                    size_t tmp_len = strlen(tmp) + 256;
                    char *buf = calloc(tmp_len + 1, 1);
                    if (buf) {
                        const char *sr = tmp;
                        size_t wp = 0;
                        while (*sr) {
                            if (sr[0] == '\\' && sr[1] == 'r') {
                                buf[wp++] = '\r';
                                sr += 2;
                            } else {
                                buf[wp++] = *sr++;
                            }
                            if (wp + 4 >= tmp_len) {
                                tmp_len += 256;
                                char *nb = realloc(buf, tmp_len + 1);
                                if (!nb) break;
                                buf = nb;
                            }
                        }
                        buf[wp] = '\0';
                        free(unescaped_new_str);
                        unescaped_new_str = buf;
                    }
                }
                if (unescaped_new_str) {
                    effective_new_str = unescaped_new_str;
                }
            }
        }
    }

    /* Unicode preservation (mirrors Python _preserve_unicode_in_replacement):
     * for a unicode_normalized match, keep the file's original Unicode chars in
     * unchanged spans (e.g. smart quotes stay smart) instead of writing the
     * ASCII-normalized new_string verbatim. */
    if (strcmp(strategy_used, "unicode_normalized") == 0 && effective_new_str[0]) {
        char *region = (char *)malloc(match_length + 1);
        if (region) {
            memcpy(region, content + match_offset, match_length);
            region[match_length] = '\0';
            char *pres = _preserve_unicode_in_replacement(region, old_str, effective_new_str);
            free(region);
            if (pres) {
                if (unescaped_new_str) { free(unescaped_new_str); unescaped_new_str = NULL; }
                effective_new_str = pres;
                unescaped_new_str = pres;
            }
        }
    }

    /* Indent-realignment (mirrors Python _reindent_replacement): when the match
     * came from a non-exact fuzzy strategy, shift effective_new_str so its
     * indentation matches the file region's actual base indent. */
    if (strcmp(strategy_used, "exact") != 0 && effective_new_str[0]) {
        char *region = (char *)malloc(match_length + 1);
        if (region) {
            memcpy(region, content + match_offset, match_length);
            region[match_length] = '\0';
            char *reind = _reindent_replacement(region, old_str, effective_new_str);
            free(region);
            if (reind) {
                if (unescaped_new_str) { free(unescaped_new_str); unescaped_new_str = NULL; }
                effective_new_str = reind;
                unescaped_new_str = reind; /* freed below via unescaped_new_str */
            }
        }
    }

    /* Calculate new content size */
    size_t new_len = strlen(effective_new_str);
    size_t result_size = bytes_read + (size_t)count * (new_len - old_len) + 1;
    char *result = (char *)malloc(result_size);
    if (!result) { free(content); return strdup("{\"error\":\"OOM\"}"); }

    /* Build result */
    size_t pos = 0;
    const char *src = content;
    int replacements = 0;

    while (replacements < count) {
        const char *match;
        if (replacements == 0 && count == 1 && strcmp(strategy_used, "exact") != 0) {
            /* Fuzzy match — use pre-computed position */
            match = content + match_offset;
        } else {
            match = strstr(src, old_str);
            if (!match) break;
        }

        /* Copy before match */
        size_t before = (size_t)(match - src);
        memcpy(result + pos, src, before);
        pos += before;

        /* Copy new string */
        if (new_len > 0) {
            memcpy(result + pos, effective_new_str, new_len);
            pos += new_len;
        }

        if (replacements == 0 && count == 1 && strcmp(strategy_used, "exact") != 0) {
            src = match + match_length;
        } else {
            src = match + old_len;
        }
        replacements++;

        if (!replace_all) break;
    }

    /* Copy remaining */
    size_t remaining = strlen(src);
    memcpy(result + pos, src, remaining);
    pos += remaining;
    result[pos] = '\0';

    /* Write back (or preview in dry_run mode) */
    size_t written = 0;
    if (!dry_run) {
        f = fopen(path, "w");
        if (!f) {
            free(content);
            free(unescaped_new_str);
            free(result);
            return strdup("{\"error\":\"Cannot open file for writing\"}");
        }
        written = fwrite(result, 1, pos, f);
        fclose(f);
    }

    /* Build JSON result with diff */
    json_node_t *r = json_new_object();
    json_object_set(r, "success", json_new_bool(true));
    json_object_set(r, "replacements", json_new_number((double)replacements));
    json_object_set(r, "strategy", json_new_string(strategy_used));
    json_object_set(r, "dry_run", json_new_bool(dry_run));
    json_object_set(r, "bytes_written", json_new_number((double)written));

    /* Show unified diff (simple: show first 200 chars of old/new) */
    char diff_buf[1024];
    size_t show_old = strlen(old_str) > 200 ? 200 : strlen(old_str);
    size_t show_new = new_len > 200 ? 200 : new_len;
    snprintf(diff_buf, sizeof(diff_buf),
             "--- a/%s\n+++ b/%s\n@@ -1 +1 @@\n-%.*s\n+%.*s",
             path, path, (int)show_old, old_str, (int)show_new, effective_new_str);
    json_object_set(r, "diff", json_new_string(diff_buf));

    char *json_out = json_serialize(r);
    json_free(r);
    free(content);
    free(unescaped_new_str);
    free(result);
    return json_out;
}

/* ================================================================
 *  Handler
 * ================================================================ */

/* PoP: _handle_patch @ src/tools/patch.c:patch_handler */
/* Port of Python tools/file_operations.py:patch_tool(). */
char *patch_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    if (!args_json) return strdup("{\"error\":\"No arguments provided\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) {
        /* Return valid JSON even if error message contains special chars */
        json_node_t *e = json_new_object();
        json_object_set(e, "error", json_new_string("JSON parse error"));
        if (err) json_object_set(e, "detail", json_new_string(err));
        free(err);
        char *out = json_serialize(e);
        json_free(e);
        if (!out) out = strdup("{\"error\":\"JSON parse error\"}");
        return out;
    }

    const char *mode = json_object_get_string(args, "mode", "replace");
    bool dry_run = json_object_get_bool(args, "dry_run", false);

    if (strcmp(mode, "patch") == 0) {
        const char *patch_content = json_object_get_string(args, "patch", NULL);
        char *patch_dup = patch_content ? strdup(patch_content) : NULL;
        json_free(args);
        if (!patch_dup) {
            return strdup("{\"error\":\"patch content required for mode='patch'\"}");
        }
        char *result = patch_apply_v4a(patch_dup, dry_run);
        free(patch_dup);
        return result;
    }

    /* Default: replace mode */
    const char *path = json_object_get_string(args, "path", NULL);
    const char *old_string = json_object_get_string(args, "old_string", NULL);
    const char *new_string = json_object_get_string(args, "new_string", "");
    bool replace_all = json_object_get_bool(args, "replace_all", false);

    char *path_dup = path ? strdup(path) : NULL;
    char *old_str_dup = old_string ? strdup(old_string) : NULL;
    char *new_str_dup = new_string ? strdup(new_string) : NULL;

    json_free(args);

    char *result = apply_patch(path_dup, old_str_dup, new_str_dup, replace_all, dry_run);
    free(path_dup);
    free(old_str_dup);
    free(new_str_dup);
    return result;
}

/* Auto-registration */
void registry_init_patch(void) {
    registry_register("patch",
        "Find and replace text in a file, or apply a V4A multi-file patch. "
        "Modes: 'replace' (default) — exact string matching with 9 fuzzy fallback strategies "
        "(line_trimmed, whitespace_normalized, indentation_flexible, escape_normalized, "
        "trimmed_boundary, unicode_normalized, block_anchor, context_aware). "
        "'patch' — V4A multi-file format with *** Begin Patch / *** Update File: / "
        "*** Add File: / *** Delete File: / *** End Patch markers. "
        "Replace mode returns a diff and the matching strategy. "
        "Patch mode returns per-operation results.",
        SCHEMA_PATCH, patch_handler);
}
