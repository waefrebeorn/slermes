/*
 * rich_code_regions.c - C11 port of upstream
 * plugins/platforms/telegram/rich_code_regions.py (gate w26_richcodere_c9c20f).
 *
 * PoP: w26_richcodere_c9c20f_regions @ plugins/platforms/telegram/rich_code_regions.py:rich_code_regions
 * PoP: w26_richcodere_c9c20f_strip_code @ plugins/platforms/telegram/rich_code_regions.py:strip_code
 * PoP: w26_richcodere_c9c20f_map_outside @ plugins/platforms/telegram/rich_code_regions.py:map_outside_code
 * PoP: w26_richcodere_c9c20f_constants @ plugins/platforms/telegram/rich_code_regions.py:_FENCE_OPEN_RE/_LEADIN_FENCE_RE
 *
 * One linear pass instead of the backreference regex: lines are scanned once
 * for fences and indented blocks, backtick runs are indexed once by width so a
 * close is a bisect and never a rescan, and <pre>/<code> pairs are matched the
 * same way.  Nothing here reads a clock, a file or a socket; the text is the
 * whole input.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_richcodere_c9c20f.h"

#define RC_NONE ((size_t)-1)

typedef struct {
    size_t start, end;
} rc_span;

typedef struct {
    rc_span *v;
    size_t n, cap;
} rc_list;

static void rc_push(rc_list *l, size_t start, size_t end) {
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        rc_span *v = (rc_span *)realloc(l->v, cap * sizeof(rc_span));
        if (!v) return;
        l->v = v;
        l->cap = cap;
    }
    l->v[l->n].start = start;
    l->v[l->n].end = end;
    l->n++;
}

static int rc_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int rc_digit(char c) { return c >= '0' && c <= '9'; }

static int rc_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || rc_digit(c) || c == '_';
}

/* --- the block prefixes -------------------------------------------------- */

/* [ \t]*(?:(?:>[ \t]*|[-+*][ \t]+|\d+[.)][ \t]+))* -> index after it. */
static int rc_block_prefix(const char *s, size_t n, size_t *out) {
    size_t i = 0;

    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    for (;;) {
        size_t j, m;
        if (i < n && s[i] == '>') {
            i++;
            while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
            continue;
        }
        if (i < n && (s[i] == '-' || s[i] == '+' || s[i] == '*')) {
            j = i + 1;
            m = j;
            while (m < n && (s[m] == ' ' || s[m] == '\t')) m++;
            if (m == j) return 0;
            i = m;
            continue;
        }
        if (i < n && rc_digit(s[i])) {
            j = i;
            while (j < n && rc_digit(s[j])) j++;
            if (j < n && (s[j] == '.' || s[j] == ')')) {
                m = j + 1;
                while (m < n && (s[m] == ' ' || s[m] == '\t')) m++;
                if (m == j + 1) return 0;
                i = m;
                continue;
            }
            return 0;
        }
        break;
    }
    *out = i;
    return 1;
}

/* _FENCE_OPEN_RE: a fence a line may open. */
static int rc_fence_open(const char *s, size_t n, size_t *off, size_t *width) {
    size_t p, j, i;

    if (!rc_block_prefix(s, n, &p)) return 0;
    if (p < n && s[p] == '`') {
        j = p;
        while (j < n && s[j] == '`') j++;
        if (j - p < 3) return 0;
        for (i = j; i < n; i++)          /* (?!`[^`\n]*`) */
            if (s[i] == '`') return 0;
        *off = p;
        *width = j - p;
        return 1;
    }
    if (p < n && s[p] == '~') {
        j = p;
        while (j < n && s[j] == '~') j++;
        if (j - p < 3) return 0;
        *off = p;
        *width = j - p;
        return 1;
    }
    return 0;
}

/* _FENCE_CLOSE_RE over s[start:end): prefix, fence, [ \t]*\r? and the end. */
static int rc_fence_close(const char *s, size_t start, size_t end, char *ch, size_t *width) {
    size_t p, j, k;
    char c;

    if (end <= start) return 0;
    if (!rc_block_prefix(s + start, end - start, &p)) return 0;
    p += start;
    if (p >= end) return 0;
    c = s[p];
    if (c != '`' && c != '~') return 0;
    j = p;
    while (j < end && s[j] == c) j++;
    if (j - p < 3) return 0;
    k = j;
    while (k < end && (s[k] == ' ' || s[k] == '\t')) k++;
    if (k < end && s[k] == '\r') k++;
    if (k != end) return 0;
    *ch = c;
    *width = j - p;
    return 1;
}

/* _LEADIN_FENCE_RE: the last >=3 backtick run, with only an info word and
 * trailing blanks after it.  A mid-line run in prose is literal text. */
static int rc_leadin_fence(const char *s, size_t n, size_t *off, size_t *width) {
    size_t i = 0, best = RC_NONE, best_w = 0, k;

    while (i < n) {
        if (s[i] == '`') {
            size_t j = i;
            while (j < n && s[j] == '`') j++;
            if (j - i >= 3) { best = i; best_w = j - i; }
            i = j;
        } else {
            i++;
        }
    }
    if (best == RC_NONE) return 0;
    k = best + best_w;
    while (k < n && s[k] != '`' && !rc_ws(s[k])) k++;
    while (k < n && (s[k] == ' ' || s[k] == '\t')) k++;
    if (k < n && s[k] == '\r') k++;
    if (k != n) return 0;
    *off = best;
    *width = best_w;
    return 1;
}

/* _LIST_ITEM_RE: [ \t]*(?:[-+*]|\d+[.)])[ \t] */
static int rc_list_item(const char *s, size_t n) {
    size_t i = 0;

    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i < n && (s[i] == '-' || s[i] == '+' || s[i] == '*'))
        return i + 1 < n && (s[i + 1] == ' ' || s[i + 1] == '\t');
    if (i < n && rc_digit(s[i])) {
        size_t j = i;
        while (j < n && rc_digit(s[j])) j++;
        if (j < n && (s[j] == '.' || s[j] == ')'))
            return j + 1 < n && (s[j + 1] == ' ' || s[j + 1] == '\t');
    }
    return 0;
}

/* _INDENTED_RE: (?: {4}| {0,3}\t) */
static int rc_indented(const char *s, size_t n) {
    size_t k = 0;

    while (k < n && k < 4 && s[k] == ' ') k++;
    if (k == 4) return 1;
    return k < n && s[k] == '\t';
}

/* --- block regions ------------------------------------------------------- */

static size_t rc_line_count(const char *s, size_t n) {
    size_t i, c = 1;
    for (i = 0; i < n; i++) if (s[i] == '\n') c++;
    return c;
}

/* For each line index, the widest ``` closing-fence line after it (0 if none). */
static size_t *rc_widest_closer_after(const char *s, size_t n, const rc_span *lines, size_t nl) {
    size_t *out = (size_t *)calloc(nl ? nl : 1, sizeof(size_t));
    size_t widest = 0, i;

    if (!out) return NULL;
    for (i = nl; i-- > 0;) {
        char ch;
        size_t w;
        out[i] = widest;
        if (rc_fence_close(s, lines[i].start, lines[i].end, &ch, &w) && ch == '`' && w > widest)
            widest = w;
    }
    (void)n;
    return out;
}

static void rc_block_regions(const char *s, size_t n, rc_list *out) {
    size_t nl = rc_line_count(s, n), i, li = 0;
    rc_span *lines = (rc_span *)malloc(nl * sizeof(rc_span));
    size_t *closer_after = NULL;
    char fence_ch = '\0';
    size_t fence_w = 0, fence_start = 0, indented_start = RC_NONE;
    int prev_blank = 1, in_list = 0;

    if (!lines) return;
    {
        size_t start = 0;
        for (i = 0; i < n; i++) {
            if (s[i] == '\n') {
                lines[li].start = start;
                lines[li].end = i;
                li++;
                start = i + 1;
            }
        }
        lines[li].start = start;
        lines[li].end = n;
        li++;
    }

    for (i = 0; i < li; i++) {
        size_t start = lines[i].start, end = lines[i].end, len = end - start;
        const char *line = s + start;
        int blank;
        size_t off = 0, width = 0;
        int opened = 0;

        if (fence_ch) {
            char ch;
            size_t w;
            if (rc_fence_close(s, start, end, &ch, &w) && ch == fence_ch && w >= fence_w) {
                rc_push(out, fence_start, end);
                fence_ch = '\0';
                prev_blank = 0;
            }
            continue;
        }
        blank = 1;
        for (size_t q = 0; q < len; q++)
            if (!rc_ws(line[q])) { blank = 0; break; }
        if (indented_start != RC_NONE && !blank && !rc_indented(line, len)) {
            if (start > 0) rc_push(out, indented_start, start - 1);
            indented_start = RC_NONE;
        }
        opened = rc_fence_open(line, len, &off, &width);
        if (!opened && rc_leadin_fence(line, len, &off, &width)) {
            if (!closer_after) closer_after = rc_widest_closer_after(s, n, lines, li);
            opened = !closer_after || closer_after[i] >= width;
        }
        if (indented_start == RC_NONE && opened) {
            fence_ch = s[start + off];
            fence_w = width;
            fence_start = start + off;
            continue;
        }
        if (indented_start == RC_NONE && !blank && prev_blank && !in_list &&
            rc_indented(line, len)) {
            indented_start = start;
        } else if (!blank && indented_start == RC_NONE) {
            in_list = rc_list_item(line, len) || (in_list && (line[0] == ' ' || line[0] == '\t'));
        }
        prev_blank = blank;
    }
    if (fence_ch) {
        rc_push(out, fence_start, n);
    } else if (indented_start != RC_NONE) {
        rc_push(out, indented_start, n);
    }
    free(closer_after);
    free(lines);
}

/* --- inline regions ------------------------------------------------------ */

static size_t rc_bisect(const size_t *v, size_t n, size_t x) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (v[mid] < x) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

/* <(pre|code)\b[^<>\n]*>  -> end offset (exclusive), name 0=pre 1=code */
static int rc_html_open_at(const char *s, size_t n, size_t i, size_t *end, int *name) {
    size_t j;

    if (i >= n || s[i] != '<') return 0;
    j = i + 1;
    if (j + 3 <= n && (s[j] == 'p' || s[j] == 'P') && (s[j + 1] == 'r' || s[j + 1] == 'R') &&
        (s[j + 2] == 'e' || s[j + 2] == 'E')) {
        size_t k = j + 3;
        if (k < n && rc_word(s[k])) return 0;
        *name = 0;
        j = k;
    } else if (j + 4 <= n && (s[j] == 'c' || s[j] == 'C') && (s[j + 1] == 'o' || s[j + 1] == 'O') &&
               (s[j + 2] == 'd' || s[j + 2] == 'D') && (s[j + 3] == 'e' || s[j + 3] == 'E')) {
        size_t k = j + 4;
        if (k < n && rc_word(s[k])) return 0;
        *name = 1;
        j = k;
    } else {
        return 0;
    }
    while (j < n && s[j] != '<' && s[j] != '>' && s[j] != '\n') j++;
    if (j >= n || s[j] != '>') return 0;
    *end = j + 1;
    return 1;
}

/* </(pre|code)[ \t]*>  -> position of the '>' */
static int rc_html_close_at(const char *s, size_t n, size_t i, int *name, size_t *gt) {
    size_t j;

    if (i + 1 >= n || s[i] != '<' || s[i + 1] != '/') return 0;
    j = i + 2;
    if (j + 3 <= n && (s[j] == 'p' || s[j] == 'P') && (s[j + 1] == 'r' || s[j + 1] == 'R') &&
        (s[j + 2] == 'e' || s[j + 2] == 'E')) {
        *name = 0;
        j += 3;
    } else if (j + 4 <= n && (s[j] == 'c' || s[j] == 'C') && (s[j + 1] == 'o' || s[j + 1] == 'O') &&
               (s[j + 2] == 'd' || s[j + 2] == 'D') && (s[j + 3] == 'e' || s[j + 3] == 'E')) {
        *name = 1;
        j += 4;
    } else {
        return 0;
    }
    while (j < n && (s[j] == ' ' || s[j] == '\t')) j++;
    if (j >= n || s[j] != '>') return 0;
    *gt = j;
    return 1;
}

static void rc_inline_regions(const char *s, size_t n, size_t lo, size_t hi, int html,
                              rc_list *out) {
    size_t *rs = NULL, *re = NULL, *rw = NULL, *next_same = NULL;
    size_t nr = 0, cap = 0, i, pos, k = 0, h = 0;
    size_t *blank = NULL, nb = 0;
    size_t *close_pre = NULL, *close_code = NULL, ncp = 0, ncc = 0;
    size_t *ho = NULL, *he = NULL, *hn = NULL, nhtml = 0;

    if (hi <= lo) return;

    for (i = lo; i < hi;) {
        if (s[i] == '`') {
            size_t j = i;
            while (j < hi && s[j] == '`') j++;
            if (nr == cap) {
                size_t ncap = cap ? cap * 2 : 16;
                size_t *a = (size_t *)realloc(rs, ncap * sizeof(size_t));
                size_t *b = (size_t *)realloc(re, ncap * sizeof(size_t));
                size_t *c = (size_t *)realloc(rw, ncap * sizeof(size_t));
                if (!a || !b || !c) { free(a); free(b); free(c); goto done; }
                rs = a; re = b; rw = c; cap = ncap;
            }
            rs[nr] = i;
            re[nr] = j;
            rw[nr] = j - i;
            nr++;
            i = j;
        } else {
            i++;
        }
    }

    if (nr) {
        next_same = (size_t *)malloc(nr * sizeof(size_t));
        if (!next_same) goto done;
        /* next_same[i] = smallest j > i with the same run width (RC_NONE if none).
         * One backward pass over a tiny width table: no rescan of the text. */
        {
            size_t *lw = (size_t *)malloc(nr * sizeof(size_t));
            size_t *li = (size_t *)malloc(nr * sizeof(size_t));
            size_t nl = 0, q;
            if (!lw || !li) { free(lw); free(li); goto done; }
            for (q = nr; q-- > 0;) {
                size_t w = rw[q], z, found = RC_NONE;
                for (z = 0; z < nl; z++) {
                    if (lw[z] == w) { found = li[z]; break; }
                }
                next_same[q] = found;
                if (z < nl) li[z] = q;
                else { lw[nl] = w; li[nl] = q; nl++; }
            }
            free(lw);
            free(li);
        }
    }

    for (i = lo; i < hi;) {
        if (s[i] == '\n') {
            size_t j = i + 1, q;
            while (j < hi && (s[j] == ' ' || s[j] == '\t')) j++;
            if (j < hi && s[j] == '\n') {
                size_t *nbuf = (size_t *)realloc(blank, (nb + 1) * sizeof(size_t));
                if (!nbuf) goto done;
                blank = nbuf;
                blank[nb++] = i;
            }
            q = i + 1;
            i = q;
        } else {
            i++;
        }
    }

    for (i = lo; i < hi;) {
        int name;
        size_t gt;
        if (rc_html_close_at(s, hi, i, &name, &gt)) {
            if (name == 0) {
                size_t *nbuf = (size_t *)realloc(close_pre, (ncp + 1) * sizeof(size_t));
                if (!nbuf) goto done;
                close_pre = nbuf;
                close_pre[ncp++] = i;
            } else {
                size_t *nbuf = (size_t *)realloc(close_code, (ncc + 1) * sizeof(size_t));
                if (!nbuf) goto done;
                close_code = nbuf;
                close_code[ncc++] = i;
            }
            i = gt + 1;
        } else {
            i++;
        }
    }

    if (html) {
        for (i = lo; i < hi;) {
            size_t end;
            int name;
            if (rc_html_open_at(s, hi, i, &end, &name)) {
                size_t *a = (size_t *)realloc(ho, (nhtml + 1) * sizeof(size_t));
                size_t *b = (size_t *)realloc(he, (nhtml + 1) * sizeof(size_t));
                size_t *c = (size_t *)realloc(hn, (nhtml + 1) * sizeof(size_t));
                if (!a || !b || !c) goto done;
                ho = a; he = b; hn = c;
                ho[nhtml] = i;
                he[nhtml] = end;
                hn[nhtml] = (size_t)name;
                nhtml++;
                i = end;
            } else {
                i++;
            }
        }
    }

    pos = lo;
    while (k < nr || h < nhtml) {
        if (h < nhtml && (k == nr || ho[h] < rs[k])) {
            size_t start = ho[h], open_end = he[h];
            int name = (int)hn[h];
            size_t *ends = name == 0 ? close_pre : close_code;
            size_t ne = name == 0 ? ncp : ncc, j, gt;
            h++;
            if (start < pos) continue;
            j = rc_bisect(ends, ne, open_end);
            if (j < ne) {
                gt = ends[j];
                while (gt < n && s[gt] != '>') gt++;
                if (gt < n) {
                    rc_push(out, start, gt + 1);
                    pos = gt + 1;
                }
            }
            continue;
        }
        {
            size_t start = rs[k], end = re[k], close_start, close_end, j, b;
            size_t idx = k;
            k++;
            if (start < pos) continue;
            j = next_same[idx];
            if (j == RC_NONE) continue;
            close_start = rs[j];
            close_end = re[j];
            b = rc_bisect(blank, nb, end);
            if (b < nb && blank[b] < close_start) continue;   /* a span ends with its paragraph */
            rc_push(out, start, close_end);
            pos = close_end;
            k = j + 1;
        }
    }

done:
    free(rs);
    free(re);
    free(rw);
    free(next_same);
    free(blank);
    free(close_pre);
    free(close_code);
    free(ho);
    free(he);
    free(hn);
}

/* --- the public surface -------------------------------------------------- */

static const char *rc_text(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

static int rc_flag(const json_t *v, int def) {
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_NULL) return 0;
    return def;
}

json_t *w26_richcodere_c9c20f_regions(const json_t *text, const json_t *html) {
    const char *s = rc_text(text);
    rc_list blocks = {NULL, 0, 0}, regions = {NULL, 0, 0};
    size_t n, pos = 0, i;
    json_t *out;
    int use_html = rc_flag(html, 1);

    if (!s) return json_array();
    n = strlen(s);
    rc_block_regions(s, n, &blocks);
    for (i = 0; i <= blocks.n; i++) {
        size_t start = i < blocks.n ? blocks.v[i].start : n;
        size_t end = i < blocks.n ? blocks.v[i].end : n;
        rc_inline_regions(s, n, pos, start, use_html, &regions);
        if (end > start) rc_push(&regions, start, end);
        pos = end;
    }
    out = json_array();
    for (i = 0; i < regions.n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_number((double)regions.v[i].start));
        json_append(pair, json_number((double)regions.v[i].end));
        json_append(out, pair);
    }
    free(regions.v);
    free(blocks.v);
    return out;
}

/* Regions of the same text without the HTML elements (strip_code's view). */
static rc_list rc_regions_list(const char *s, size_t n, int use_html) {
    rc_list blocks = {NULL, 0, 0}, regions = {NULL, 0, 0};
    size_t pos = 0, i;

    rc_block_regions(s, n, &blocks);
    for (i = 0; i <= blocks.n; i++) {
        size_t start = i < blocks.n ? blocks.v[i].start : n;
        size_t end = i < blocks.n ? blocks.v[i].end : n;
        rc_inline_regions(s, n, pos, start, use_html, &regions);
        if (end > start) rc_push(&regions, start, end);
        pos = end;
    }
    free(blocks.v);
    return regions;
}

typedef struct {
    char *p;
    size_t n, cap;
} rc_buf;

static int rc_buf_reserve(rc_buf *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return 1;
    {
        size_t ncap = (b->n + extra + 1) * 2;
        char *nb = (char *)realloc(b->p, ncap);
        if (!nb) return 0;
        b->p = nb;
        b->cap = ncap;
    }
    return 1;
}

static int rc_buf_put(rc_buf *b, const char *s, size_t n) {
    if (!rc_buf_reserve(b, n)) return 0;
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    return 1;
}

/* A gap copied through the named transform; NULL copies verbatim, "drop"
 * copies nothing.  "bracket" wraps, "upper"/"lower" fold ASCII. */
static int rc_buf_put_transformed(rc_buf *b, const char *s, size_t n, const char *transform) {
    size_t q;

    if (transform && !strcmp(transform, "drop")) return 1;
    if (transform && !strcmp(transform, "bracket")) {
        if (!rc_buf_put(b, "[", 1)) return 0;
    }
    if (!rc_buf_reserve(b, n)) return 0;
    for (q = 0; q < n; q++) {
        char c = s[q];
        if (transform && !strcmp(transform, "upper")) c = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
        else if (transform && !strcmp(transform, "lower")) c = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
        b->p[b->n++] = c;
    }
    if (transform && !strcmp(transform, "bracket")) {
        if (!rc_buf_put(b, "]", 1)) return 0;
    }
    return 1;
}

/* map_outside_code (keep_regions) or strip_code (!keep_regions). */
static json_t *rc_apply(const char *s, rc_list *regions, const char *transform,
                        int keep_regions) {
    size_t n = strlen(s), pos = 0, i;
    rc_buf b = {NULL, 0, 0};
    json_t *out;

    if (!rc_buf_reserve(&b, n)) return json_string("");
    for (i = 0; i < regions->n; i++) {
        size_t gs = regions->v[i].start, ge = regions->v[i].end;
        size_t gap_n = gs > pos ? gs - pos : 0;
        if (!rc_buf_put_transformed(&b, s + pos, gap_n, keep_regions ? transform : NULL) ||
            (keep_regions && !rc_buf_put(&b, s + gs, ge > gs ? ge - gs : 0))) {
            free(b.p);
            return json_string("");
        }
        pos = ge;
    }
    if (!rc_buf_put_transformed(&b, s + pos, n > pos ? n - pos : 0,
                                keep_regions ? transform : NULL)) {
        free(b.p);
        return json_string("");
    }
    b.p[b.n] = '\0';
    out = json_string(b.p);
    free(b.p);
    return out;
}

json_t *w26_richcodere_c9c20f_strip_code(const json_t *text) {
    const char *s = rc_text(text);
    rc_list regions;
    json_t *out;

    if (!s) return json_string("");
    regions = rc_regions_list(s, strlen(s), 0);
    out = rc_apply(s, &regions, NULL, 0);
    free(regions.v);
    return out;
}

json_t *w26_richcodere_c9c20f_map_outside(const json_t *text, const json_t *transform,
                                          const json_t *html) {
    const char *s = rc_text(text), *t = rc_text(transform);
    rc_list regions;
    json_t *out;

    if (!s) return json_string("");
    if (!t) t = "identity";
    regions = rc_regions_list(s, strlen(s), rc_flag(html, 1));
    out = rc_apply(s, &regions, t, 1);
    free(regions.v);
    return out;
}

json_t *w26_richcodere_c9c20f_constants(void) {
    json_t *o = json_object();
    json_set(o, "fence_min_width", json_number(3.0));
    json_set(o, "block_prefix_groups", json_number(3.0));
    return o;
}
