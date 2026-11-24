/*
 * base_chunking.c - C home for gateway/platforms/base_chunking.py.
 * Markdown-aware chunking: fence re-opening, table header repetition, inline-code
 * and Markdown-link retreat, and the (N/M) indicators. All string work is done in
 * codepoint space because Python indexes str by codepoint; the fence/table
 * primitives are ported here from gateway/platforms/helpers.py so the port stands
 * alone.
 * PoP: w19_basechunki_b0f787_chunk_markdown_message @ gateway/platforms/base_chunking.py:chunk_markdown_message
 * PoP: w19_basechunki_b0f787_open_link_bracket @ gateway/platforms/base_chunking.py:_open_link_bracket
 * PoP: w19_basechunki_b0f787_fence_state_after @ gateway/platforms/helpers.py:fence_state_after
 * PoP: w19_basechunki_b0f787_table_row_start @ gateway/platforms/helpers.py:table_row_start
 * PoP: w19_basechunki_b0f787_table_rollback_point @ gateway/platforms/helpers.py:table_rollback_point
 * PoP: w19_basechunki_b0f787_table_header_to_repeat @ gateway/platforms/helpers.py:table_header_to_repeat
 */
#include "noc_w19_basechunki_b0f787.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BC_INDICATOR_RESERVE 12
#define BC_FENCE_CLOSE "\n```"
#define BC_FENCE_CLOSE_LEN 4

/* ---------------- codepoint text ---------------- */

typedef struct { uint32_t *cp; size_t n; } bc_text;

static char *bc_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static size_t bc_encode(uint32_t cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static bc_text bc_from_utf8(const char *s) {
    bc_text t;
    size_t i = 0, len;
    t.cp = NULL;
    t.n = 0;
    if (!s) return t;
    len = strlen(s);
    t.cp = (uint32_t *)malloc((len + 1) * sizeof(uint32_t));
    if (!t.cp) return t;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp;
        size_t step = 1;
        if (c < 0x80) cp = c;
        else if ((c & 0xE0) == 0xC0 && s[i + 1]) {
            cp = ((uint32_t)(c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
            step = 2;
        } else if ((c & 0xF0) == 0xE0 && s[i + 1] && s[i + 2]) {
            cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)((unsigned char)s[i + 1] & 0x3F) << 6)
               | ((unsigned char)s[i + 2] & 0x3F);
            step = 3;
        } else if ((c & 0xF8) == 0xF0 && s[i + 1] && s[i + 2] && s[i + 3]) {
            cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)((unsigned char)s[i + 1] & 0x3F) << 12)
               | ((uint32_t)((unsigned char)s[i + 2] & 0x3F) << 6)
               | ((unsigned char)s[i + 3] & 0x3F);
            step = 4;
        } else cp = c;
        i += step;
        t.cp[t.n++] = cp;
    }
    return t;
}

static bc_text bc_slice(const bc_text *src, size_t start, size_t end) {
    bc_text out;
    out.n = 0;
    out.cp = NULL;
    if (!src || !src->cp || start >= end || end > src->n) return out;
    out.cp = (uint32_t *)malloc((end - start) * sizeof(uint32_t));
    if (!out.cp) return out;
    memcpy(out.cp, src->cp + start, (end - start) * sizeof(uint32_t));
    out.n = end - start;
    return out;
}

static void bc_free(bc_text *t) {
    free(t->cp);
    t->cp = NULL;
    t->n = 0;
}

static char *bc_to_utf8(const bc_text *t) {
    char *s;
    size_t i, at = 0;
    if (!t || !t->cp) return bc_dup("");
    s = (char *)malloc(t->n * 4 + 1);
    if (!s) return NULL;
    for (i = 0; i < t->n; i++) at += bc_encode(t->cp[i], s + at);
    s[at] = '\0';
    return s;
}

/* ---------------- string builder ---------------- */

typedef struct { char *s; size_t n, cap; } bc_sb;

static void sb_init(bc_sb *b) { b->s = NULL; b->n = 0; b->cap = 0; }

static void sb_reserve(bc_sb *b, size_t extra) {
    if (b->n + extra + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 64;
        char *ns;
        while (cap < b->n + extra + 1) cap *= 2;
        ns = (char *)realloc(b->s, cap);
        if (!ns) return;
        b->s = ns;
        b->cap = cap;
    }
    if (b->s && b->n < b->cap) b->s[b->n] = '\0';
}

static void sb_add_str(bc_sb *b, const char *s) {
    size_t n;
    if (!s || !b->s || !b->cap) {
        if (!b->cap) { sb_init(b); sb_reserve(b, strlen(s ? s : "")); }
        else return;
    }
    n = strlen(s);
    sb_reserve(b, n);
    memcpy(b->s + b->n, s, n);
    b->n += n;
    b->s[b->n] = '\0';
}

static void sb_add_range(bc_sb *b, const bc_text *t, size_t start, size_t end) {
    size_t i;
    char buf[4];
    if (!t || start >= end) return;
    if (end > t->n) end = t->n;
    sb_reserve(b, (end - start) * 4);
    for (i = start; i < end; i++) {
        size_t k = bc_encode(t->cp[i], buf);
        memcpy(b->s + b->n, buf, k);
        b->n += k;
    }
    b->s[b->n] = '\0';
}

static char *sb_take(bc_sb *b) {
    char *s = b->s ? b->s : bc_dup("");
    b->s = NULL;
    b->n = b->cap = 0;
    return s;
}

/* ---------------- measures ---------------- */

static size_t bc_utf16_len(const bc_text *t) {
    size_t i, n = 0;
    if (!t || !t->cp) return 0;
    for (i = 0; i < t->n; i++) n += (t->cp[i] >= 0x10000) ? 2 : 1;
    return n;
}

static size_t bc_measure(const bc_text *t, int utf16) {
    return utf16 ? bc_utf16_len(t) : (t ? t->n : 0);
}

static size_t bc_measure_prefix(const bc_text *t, size_t end, int utf16) {
    size_t i, n = 0;
    if (!t || !t->cp) return 0;
    if (end > t->n) end = t->n;
    for (i = 0; i < end; i++) n += (utf16 && t->cp[i] >= 0x10000) ? 2 : 1;
    return n;
}

static size_t bc_measure_str(const char *s, int utf16) {
    bc_text t = bc_from_utf8(s ? s : "");
    size_t n = bc_measure(&t, utf16);
    bc_free(&t);
    return n;
}

/* Largest codepoint offset n with len_fn(text[:n]) <= budget (binary search). */
static size_t bc_custom_unit_to_cp(const bc_text *t, size_t budget, int utf16) {
    size_t lo = 0, hi;
    if (!t || !t->cp) return 0;
    if (bc_measure(t, utf16) <= budget) return t->n;
    hi = t->n;
    while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        if (bc_measure_prefix(t, mid, utf16) <= budget) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

static int bc_isspace(uint32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static long bc_rfind(const bc_text *t, uint32_t ch, size_t from, size_t to) {
    size_t i;
    if (!t || !t->cp) return -1;
    if (to > t->n) to = t->n;
    i = to;
    while (i > from) {
        i--;
        if (t->cp[i] == ch) return (long)i;
    }
    return -1;
}

static long bc_find(const bc_text *t, uint32_t ch, size_t from) {
    size_t i;
    if (!t || !t->cp) return -1;
    for (i = from; i < t->n; i++)
        if (t->cp[i] == ch) return (long)i;
    return -1;
}

static size_t bc_count_char(const bc_text *t, size_t from, size_t to, uint32_t ch) {
    size_t i, n = 0;
    if (!t || !t->cp) return 0;
    if (to > t->n) to = t->n;
    for (i = from; i < to; i++)
        if (t->cp[i] == ch) n++;
    return n;
}

/* text.count("\\`") - non-overlapping occurrences of the two-character sequence. */
static size_t bc_count_escaped_backticks(const bc_text *t, size_t from, size_t to) {
    size_t i = from, n = 0;
    if (!t || !t->cp) return 0;
    if (to > t->n) to = t->n;
    while (i + 1 < to) {
        if (t->cp[i] == '\\' && t->cp[i + 1] == '`') { n++; i += 2; }
        else i++;
    }
    return n;
}

/* ---------------- fence state ---------------- */

static void bc_fence_state(const bc_text *t, int in_code, const char *lang, int *out_code,
                           char **out_lang) {
    char *cur = bc_dup(lang ? lang : "");
    size_t line_start = 0;
    int state = in_code ? 1 : 0;
    while (line_start <= t->n) {
        size_t line_end = line_start, a, b;
        while (line_end < t->n && t->cp[line_end] != '\n') line_end++;
        a = line_start;
        b = line_end;
        while (a < b && bc_isspace(t->cp[a])) a++;
        while (b > a && bc_isspace(t->cp[b - 1])) b--;
        if (b - a >= 3 && t->cp[a] == '`' && t->cp[a + 1] == '`' && t->cp[a + 2] == '`') {
            size_t tag = a + 3, tag_end;
            char *first = NULL;
            while (tag < b && bc_isspace(t->cp[tag])) tag++;
            tag_end = tag;
            while (tag_end < b && !bc_isspace(t->cp[tag_end])) tag_end++;
            if (tag_end > tag) {
                bc_text slice = bc_slice(t, tag, tag_end);
                first = bc_to_utf8(&slice);
                bc_free(&slice);
            }
            if (state) { state = 0; free(cur); cur = bc_dup(""); }
            else { state = 1; free(cur); cur = bc_dup(first ? first : ""); }
            free(first);
        }
        if (line_end >= t->n) break;
        line_start = line_end + 1;
    }
    *out_code = state;
    *out_lang = cur;
}

json_t *w19_basechunki_b0f787_fence_state_after(const char *text, int in_code, const char *lang) {
    bc_text t = bc_from_utf8(text);
    char *out_lang = NULL;
    int code = 0;
    json_t *out = json_object();
    bc_fence_state(&t, in_code, lang, &code, &out_lang);
    json_set(out, "in_code", json_bool(code));
    json_set(out, "lang", json_string(out_lang ? out_lang : ""));
    free(out_lang);
    bc_free(&t);
    return out;
}

/* ---------------- table primitives ---------------- */

static int bc_is_table_row_range(const bc_text *t, size_t a, size_t b) {
    size_t i;
    while (a < b && bc_isspace(t->cp[a])) a++;
    while (b > a && bc_isspace(t->cp[b - 1])) b--;
    for (i = a; i < b; i++)
        if (t->cp[i] == '|') return 1;
    return 0;
}

/* TABLE_SEPARATOR_RE: ^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$ */
static int bc_is_separator_range(const bc_text *t, size_t a, size_t b) {
    size_t i = a;
    int pipes = 0;
    while (i < b && bc_isspace(t->cp[i])) i++;
    if (i < b && t->cp[i] == '|') i++;
    while (i < b && bc_isspace(t->cp[i])) i++;
    for (;;) {
        if (i < b && t->cp[i] == ':') i++;
        if (i >= b || t->cp[i] != '-') return 0;
        while (i < b && t->cp[i] == '-') i++;
        if (i < b && t->cp[i] == ':') i++;
        while (i < b && bc_isspace(t->cp[i])) i++;
        if (i >= b) break;
        if (t->cp[i] != '|') return 0;
        i++;
        pipes++;
        while (i < b && bc_isspace(t->cp[i])) i++;
        if (i >= b) break;
    }
    return pipes >= 1;
}

typedef struct { size_t *start; size_t *end; size_t n; } bc_lines;

static bc_lines bc_split_lines(const bc_text *t, size_t from, size_t to) {
    bc_lines l;
    size_t i, at;
    l.n = 0;
    l.start = NULL;
    l.end = NULL;
    if (!t || !t->cp) return l;
    if (to > t->n) to = t->n;
    if (from > to) from = to;
    l.start = (size_t *)malloc((to - from + 2) * sizeof(size_t));
    l.end = (size_t *)malloc((to - from + 2) * sizeof(size_t));
    if (!l.start || !l.end) { free(l.start); free(l.end); l.start = l.end = NULL; return l; }
    at = from;
    for (i = from; i <= to; i++) {
        if (i == to || t->cp[i] == '\n') {
            l.start[l.n] = at;
            l.end[l.n] = i;
            l.n++;
            at = i + 1;
            if (i == to) break;
        }
    }
    return l;
}

static void bc_lines_free(bc_lines *l) {
    free(l->start);
    free(l->end);
    l->start = l->end = NULL;
    l->n = 0;
}

static long bc_table_header_above(const bc_text *t, const bc_lines *l, size_t i) {
    while (i >= 1 && bc_is_table_row_range(t, l->start[i], l->end[i])) {
        if (bc_is_separator_range(t, l->start[i], l->end[i])
            && bc_is_table_row_range(t, l->start[i - 1], l->end[i - 1]))
            return (long)(i - 1);
        i--;
    }
    return -1;
}

static int bc_last_line_is_table_row(const bc_text *t, size_t from, size_t to) {
    size_t end = to, start;
    if (!t || !t->cp) return 0;
    if (end > t->n) end = t->n;
    while (end > from && t->cp[end - 1] == '\n') end--;
    start = end;
    while (start > from && t->cp[start - 1] != '\n') start--;
    return bc_is_table_row_range(t, start, end);
}

static size_t bc_lstrip_from(const bc_text *t, size_t from) {
    size_t i = from;
    if (!t || !t->cp) return from;
    while (i < t->n && bc_isspace(t->cp[i])) i++;
    return i;
}

static int bc_first_line_is_table_row(const bc_text *t, size_t from) {
    size_t start, end;
    if (!t || !t->cp) return 0;
    start = bc_lstrip_from(t, from);
    end = start;
    while (end < t->n && t->cp[end] != '\n') end++;
    return bc_is_table_row_range(t, start, end);
}

static int bc_split_inside_table(const bc_text *t, size_t split_at) {
    if (!t || !t->cp) return 0;
    if (split_at > t->n) split_at = t->n;
    return bc_last_line_is_table_row(t, 0, split_at) && bc_first_line_is_table_row(t, split_at);
}

static int bc_split_inside_table2(const bc_text *head, const bc_text *rest) {
    if (!head || !rest || !head->cp || !rest->cp) return 0;
    return bc_last_line_is_table_row(head, 0, head->n) && bc_first_line_is_table_row(rest, 0);
}

/* --- internal forms reused by the chunker (no JSON round-trip) --- */

static long bc_row_start(const bc_text *t, size_t split_at) {
    long newline, line_end;
    if (split_at > t->n) split_at = t->n;
    newline = bc_rfind(t, '\n', 0, split_at);
    if (newline < 1 || split_at >= t->n || t->cp[split_at] == '\n') return (long)split_at;
    line_end = bc_find(t, '\n', split_at);
    {
        size_t stop = (line_end >= 0) ? (size_t)line_end : t->n;
        bc_lines l = bc_split_lines(t, 0, stop);
        long r = (l.n > 0 && bc_table_header_above(t, &l, l.n - 1) >= 0) ? newline : (long)split_at;
        bc_lines_free(&l);
        return r;
    }
}

/* table_rollback_point(text, split_at, fits) */
static long bc_rollback(const bc_text *t, size_t split_at, long long budget, int utf16) {
    long result;
    if (split_at > t->n) split_at = t->n;
    result = (long)split_at;
    if (!bc_split_inside_table(t, split_at)) return result;
    {
        bc_lines head = bc_split_lines(t, 0, split_at);
        bc_lines tail = bc_split_lines(t, bc_lstrip_from(t, split_at), t->n);
        bc_lines all;
        size_t i, need = head.n + 2;
        long header;
        all.start = (size_t *)malloc(need * sizeof(size_t));
        all.end = (size_t *)malloc(need * sizeof(size_t));
        all.n = 0;
        for (i = 0; i < head.n; i++) {
            all.start[all.n] = head.start[i];
            all.end[all.n] = head.end[i];
            all.n++;
        }
        if (tail.n > 0) {
            all.start[all.n] = tail.start[0];
            all.end[all.n] = tail.end[0];
            all.n++;
        }
        header = bc_table_header_above(t, &all, all.n - 1);
        if (header > 0 && (size_t)header - 1 < head.n) {
            size_t start = head.end[(size_t)header - 1] + 1;
            if (header >= (long)head.n - 2) {
                result = (long)start;
            } else {
                size_t end = start, k;
                bc_lines rest = bc_split_lines(t, start, t->n);
                for (k = 0; k < rest.n; k++) {
                    if (!bc_is_table_row_range(t, rest.start[k], rest.end[k])) break;
                    end = rest.end[k] + 1;
                }
                bc_lines_free(&rest);
                {
                    size_t stop = end;
                    while (stop > start && t->cp[stop - 1] == '\n') stop--;
                    {
                        bc_text block = bc_slice(t, start, stop);
                        size_t m = bc_measure(&block, utf16);
                        bc_free(&block);
                        result = (budget >= 0 && m <= (size_t)budget) ? (long)start : (long)split_at;
                    }
                }
            }
        }
        free(all.start);
        free(all.end);
        bc_lines_free(&head);
        bc_lines_free(&tail);
    }
    return result;
}

/* table_header_to_repeat(chunk, rest, max_units, len_fn) */
static char *bc_header_to_repeat(const bc_text *c, const bc_text *r, long long max_units, int utf16) {
    char *out = NULL;
    if (!bc_split_inside_table2(c, r)) return NULL;
    {
        size_t end = c->n;
        bc_lines l;
        long header;
        while (end > 0 && c->cp[end - 1] == '\n') end--;
        l = bc_split_lines(c, 0, end);
        header = (l.n > 0) ? bc_table_header_above(c, &l, l.n - 1) : -1;
        if (header >= 0 && (size_t)header + 1 < l.n) {
            bc_sb b;
            bc_text block;
            char *s;
            sb_init(&b);
            sb_reserve(&b, 8);
            sb_add_range(&b, c, l.start[header], l.end[header]);
            sb_add_str(&b, "\n");
            sb_add_range(&b, c, l.start[header + 1], l.end[header + 1]);
            s = sb_take(&b);
            block = bc_from_utf8(s);
            if (max_units >= 0 && bc_measure(&block, utf16) <= (size_t)max_units) out = s;
            else free(s);
            bc_free(&block);
        }
        bc_lines_free(&l);
    }
    return out;
}

json_t *w19_basechunki_b0f787_table_row_start(const char *text, long long split_at_in) {
    bc_text t = bc_from_utf8(text);
    size_t split_at = (split_at_in < 0) ? 0 : (size_t)split_at_in;
    long result;
    if (split_at > t.n) split_at = t.n;
    {
        long newline = bc_rfind(&t, '\n', 0, split_at);
        if (newline < 1 || split_at >= t.n || t.cp[split_at] == '\n') {
            result = (long)split_at;
        } else {
            long line_end = bc_find(&t, '\n', split_at);
            size_t stop = (line_end >= 0) ? (size_t)line_end : t.n;
            bc_lines l = bc_split_lines(&t, 0, stop);
            result = (l.n > 0 && bc_table_header_above(&t, &l, l.n - 1) >= 0)
                   ? newline : (long)split_at;
            bc_lines_free(&l);
        }
    }
    bc_free(&t);
    return json_number((double)result);
}

json_t *w19_basechunki_b0f787_table_rollback_point(const char *text, long long split_at_in,
                                                   long long max_units, int len_fn_utf16) {
    bc_text t = bc_from_utf8(text);
    size_t split_at = (split_at_in < 0) ? 0 : (size_t)split_at_in;
    long long budget = (max_units < 0) ? -1 : max_units;
    long result;
    if (split_at > t.n) split_at = t.n;
    result = (long)split_at;
    if (bc_split_inside_table(&t, split_at)) {
        bc_lines head = bc_split_lines(&t, 0, split_at);
        bc_lines tail = bc_split_lines(&t, bc_lstrip_from(&t, split_at), t.n);
        long header;
        {
            bc_lines all;
            size_t i, need = head.n + 2;
            all.start = (size_t *)malloc(need * sizeof(size_t));
            all.end = (size_t *)malloc(need * sizeof(size_t));
            all.n = 0;
            for (i = 0; i < head.n; i++) {
                all.start[all.n] = head.start[i];
                all.end[all.n] = head.end[i];
                all.n++;
            }
            if (tail.n > 0) {
                all.start[all.n] = tail.start[0];
                all.end[all.n] = tail.end[0];
                all.n++;
            }
            header = bc_table_header_above(&t, &all, all.n - 1);
            if (header > 0 && (size_t)header <= all.n) {
                size_t start = (size_t)header;   /* placeholder, recomputed below */
                /* start = len('\n'.join(lines[:header])) + 1 */
                if (header >= 1 && (size_t)header - 1 < head.n)
                    start = head.end[(size_t)header - 1] + 1;
                if (header >= (long)head.n - 2) {
                    result = (long)start;
                } else {
                    size_t end = start, k;
                    bc_lines rest = bc_split_lines(&t, start, t.n);
                    for (k = 0; k < rest.n; k++) {
                        if (!bc_is_table_row_range(&t, rest.start[k], rest.end[k])) break;
                        end = rest.start[k] + (rest.end[k] - rest.start[k]) + 1;
                    }
                    bc_lines_free(&rest);
                    {
                        size_t stop = end, s = start;
                        while (stop > s && t.cp[stop - 1] == '\n') stop--;
                        {
                            bc_text block = bc_slice(&t, s, stop);
                            size_t m = bc_measure(&block, len_fn_utf16);
                            bc_free(&block);
                            result = (budget >= 0 && m <= (size_t)budget) ? (long)start
                                                                          : (long)split_at;
                        }
                    }
                }
            }
            free(all.start);
            free(all.end);
        }
        bc_lines_free(&head);
        bc_lines_free(&tail);
    }
    bc_free(&t);
    return json_number((double)result);
}

json_t *w19_basechunki_b0f787_table_header_to_repeat(const char *chunk, const char *rest,
                                                     long long max_units, int len_fn_utf16) {
    bc_text c = bc_from_utf8(chunk);
    bc_text r = bc_from_utf8(rest);
    json_t *out = json_null();
    if (bc_split_inside_table2(&c, &r)) {
        size_t end = c.n;
        while (end > 0 && c.cp[end - 1] == '\n') end--;
        {
            bc_lines l = bc_split_lines(&c, 0, end);
            long header = (l.n > 0) ? bc_table_header_above(&c, &l, l.n - 1) : -1;
            if (header >= 0 && (size_t)header + 1 < l.n) {
                bc_sb b;
                bc_text block;
                char *s;
                sb_init(&b);
                sb_reserve(&b, 8);
                sb_add_range(&b, &c, l.start[header], l.end[header]);
                sb_add_str(&b, "\n");
                sb_add_range(&b, &c, l.start[header + 1], l.end[header + 1]);
                s = sb_take(&b);
                block = bc_from_utf8(s);
                if (max_units >= 0 && bc_measure(&block, len_fn_utf16) <= (size_t)max_units)
                    out = json_string(s);
                bc_free(&block);
                free(s);
            }
            bc_lines_free(&l);
        }
    }
    bc_free(&c);
    bc_free(&r);
    return out;
}

/* ---------------- _open_link_bracket ---------------- */

static long bc_open_link_bracket(const bc_text *t) {
    size_t i = 0;
    long open_at = -1;
    int in_dest = 0;
    if (!t || !t->cp) return -1;
    while (i < t->n) {
        uint32_t ch = t->cp[i];
        if (ch == '\\') { i += 2; continue; }
        if (in_dest) {
            if (ch == ')') { in_dest = 0; open_at = -1; }
        } else if (ch == '[') {
            if (open_at < 0) open_at = (long)i;
        } else if (ch == ']') {
            if (open_at >= 0 && i + 1 < t->n && t->cp[i + 1] == '(') { in_dest = 1; i += 1; }
            else open_at = -1;
        }
        i += 1;
    }
    return open_at;
}

json_t *w19_basechunki_b0f787_open_link_bracket(const char *text) {
    bc_text t = bc_from_utf8(text);
    long r = bc_open_link_bracket(&t);
    bc_free(&t);
    return json_number((double)r);
}

/* ---------------- chunk_markdown_message ---------------- */

typedef struct { char **v; size_t n, cap; } bc_chunks;

static void bc_chunks_push(bc_chunks *c, char *s) {
    if (c->n == c->cap) {
        size_t cap = c->cap ? c->cap * 2 : 16;
        char **nv = (char **)realloc(c->v, cap * sizeof(char *));
        if (!nv) { free(s); return; }
        c->v = nv;
        c->cap = cap;
    }
    c->v[c->n++] = s;
}

json_t *w19_basechunki_b0f787_chunk_markdown_message(const char *content, long long max_length_in,
                                                    int len_fn_utf16) {
    bc_text src = bc_from_utf8(content);
    size_t max_length = (max_length_in < 0) ? 0 : (size_t)max_length_in;
    json_t *out = json_array();
    bc_chunks chunks;
    bc_text rem;
    char *carry_lang = NULL;
    char *carry_header = NULL;

    chunks.v = NULL;
    chunks.n = 0;
    chunks.cap = 0;

    if (bc_measure(&src, len_fn_utf16) <= max_length) {
        char *whole = bc_to_utf8(&src);
        json_append(out, json_string(whole ? whole : ""));
        free(whole);
        bc_free(&src);
        return out;
    }
    rem = bc_slice(&src, 0, src.n);
    bc_free(&src);

    while (rem.n > 0) {
        bc_sb prefix;
        size_t prefix_len, headroom, cp_limit, split_at;
        bc_text body, candidate;
        int in_code = 0;
        char *lang = NULL;
        char *full;

        sb_init(&prefix);
        sb_reserve(&prefix, 8);
        if (carry_header) {
            sb_add_str(&prefix, carry_header);
            sb_add_str(&prefix, "\n");
        } else if (carry_lang != NULL) {
            sb_add_str(&prefix, "```");
            sb_add_str(&prefix, carry_lang);
            sb_add_str(&prefix, "\n");
        }
        prefix_len = bc_measure_str(prefix.s ? prefix.s : "", len_fn_utf16);

        headroom = (max_length > (size_t)BC_INDICATOR_RESERVE + prefix_len + BC_FENCE_CLOSE_LEN)
                 ? max_length - BC_INDICATOR_RESERVE - prefix_len - BC_FENCE_CLOSE_LEN
                 : 0;
        if (max_length < BC_INDICATOR_RESERVE + prefix_len + BC_FENCE_CLOSE_LEN) headroom = 0;
        if (headroom < 1) {
            headroom = max_length / 2;
            if (headroom < 1) headroom = 1;
        }

        /* Remainder fits in one final chunk; close a reopened fence if still open. */
        if (prefix_len + bc_measure(&rem, len_fn_utf16)
            <= (max_length > BC_INDICATOR_RESERVE ? max_length - BC_INDICATOR_RESERVE : 0)) {
            bc_sb fin;
            sb_init(&fin);
            sb_reserve(&fin, 8);
            sb_add_str(&fin, prefix.s ? prefix.s : "");
            sb_add_range(&fin, &rem, 0, rem.n);
            if (carry_lang != NULL) {
                int code = 0;
                char *l2 = NULL;
                bc_fence_state(&rem, 1, carry_lang, &code, &l2);
                free(l2);
                if (code) sb_add_str(&fin, BC_FENCE_CLOSE);
            }
            full = sb_take(&fin);
            bc_chunks_push(&chunks, full);
            break;
        }

        cp_limit = len_fn_utf16 ? bc_custom_unit_to_cp(&rem, headroom, 1)
                                : (headroom < rem.n ? headroom : rem.n);
        {
            bc_text region = bc_slice(&rem, 0, cp_limit < rem.n ? cp_limit : rem.n);
            long nl = bc_rfind(&region, '\n', 0, region.n);
            split_at = (nl >= 0) ? (size_t)nl : 0;
            if (split_at < cp_limit / 2) {
                long sp = bc_rfind(&region, ' ', 0, region.n);
                split_at = (sp >= 0) ? (size_t)sp : 0;
            }
            bc_free(&region);
        }
        if (split_at < 1) split_at = (cp_limit > 1) ? cp_limit : 1;
        if (split_at > rem.n) split_at = rem.n;

        /* Don't split inside an inline code span or an open Markdown link. */
        for (;;) {
            size_t bt, last_bt;
            long link_lb;
            int moved = 0;
            candidate = bc_slice(&rem, 0, split_at);
            bt = bc_count_char(&candidate, 0, candidate.n, '`')
               - bc_count_escaped_backticks(&candidate, 0, candidate.n);
            if (bt % 2 == 1) {
                long lb = bc_rfind(&candidate, '`', 0, candidate.n);
                while (lb > 0 && candidate.cp[lb - 1] == '\\')
                    lb = bc_rfind(&candidate, '`', 0, (size_t)lb);
                if (lb > 0) {
                    long sp = bc_rfind(&candidate, ' ', 0, (size_t)lb);
                    long nl = bc_rfind(&candidate, '\n', 0, (size_t)lb);
                    long safe = (sp > nl) ? sp : nl;
                    if (safe > (long)(cp_limit / 4)) { split_at = (size_t)safe; moved = 1; }
                }
            }
            link_lb = bc_open_link_bracket(&candidate);
            if (link_lb > 0) {
                long sp = bc_rfind(&candidate, ' ', 0, (size_t)link_lb);
                long nl = bc_rfind(&candidate, '\n', 0, (size_t)link_lb);
                long safe = (sp > nl) ? sp : nl;
                if (safe > (long)(cp_limit / 4)) { split_at = (size_t)safe; moved = 1; }
            }
            bc_free(&candidate);
            (void)last_bt;
            if (!moved) break;
        }

        /* Pipe tables: move a mid-table split to before the header when it fits. */
        {
            int code = 0;
            char *l2 = NULL;
            body = bc_slice(&rem, 0, split_at);
            bc_fence_state(&body, carry_lang != NULL, carry_lang ? carry_lang : "", &code, &l2);
            free(l2);
            if (!code) {
                if (carry_header) {
                    bc_sb joined;
                    bc_text jt, pt;
                    size_t plen;
                    long row_start;
                    sb_init(&joined);
                    sb_reserve(&joined, 8);
                    sb_add_str(&joined, prefix.s ? prefix.s : "");
                    sb_add_range(&joined, &rem, 0, rem.n);
                    jt = bc_from_utf8(sb_take(&joined));
                    pt = bc_from_utf8(prefix.s ? prefix.s : "");
                    plen = pt.n;
                    bc_free(&pt);
                    row_start = bc_row_start(&jt, plen + split_at) - (long)plen;
                    split_at = (row_start >= 1) ? (size_t)row_start : split_at;
                    bc_free(&jt);
                } else {
                    long rs = bc_row_start(&rem, split_at);
                    split_at = (rs >= 0) ? (size_t)rs : split_at;
                }
                split_at = (size_t)bc_rollback(
                    &rem, split_at,
                    (long long)(max_length > BC_INDICATOR_RESERVE
                                ? max_length - BC_INDICATOR_RESERVE : 0), len_fn_utf16);
            }
            bc_free(&body);
        }

        {
            bc_sb fullb;
            bc_text chunk_body = bc_slice(&rem, 0, split_at);
            sb_init(&fullb);
            sb_reserve(&fullb, 8);
            sb_add_str(&fullb, prefix.s ? prefix.s : "");
            sb_add_range(&fullb, &chunk_body, 0, chunk_body.n);
            full = sb_take(&fullb);
            /* remaining = remaining[split_at:].lstrip() */
            {
                size_t nxt = bc_lstrip_from(&rem, split_at);
                bc_text tail = bc_slice(&rem, nxt, rem.n);
                bc_free(&rem);
                rem = tail;
            }
            bc_fence_state(&chunk_body, carry_lang != NULL, carry_lang ? carry_lang : "",
                           &in_code, &lang);
            free(carry_lang);
            carry_lang = in_code ? bc_dup(lang ? lang : "") : NULL;
            free(lang);
            free(carry_header);
            carry_header = NULL;
            if (!in_code) {
                bc_text fullt = bc_from_utf8(full);
                char *hdr = bc_header_to_repeat(&fullt, &rem, (long long)(max_length / 4),
                                                len_fn_utf16);
                if (hdr) carry_header = hdr;
                bc_free(&fullt);
            }
            if (in_code) {
                bc_sb closeb;
                char *with_close;
                sb_init(&closeb);
                sb_reserve(&closeb, BC_FENCE_CLOSE_LEN + 1);
                sb_add_str(&closeb, full);
                sb_add_str(&closeb, BC_FENCE_CLOSE);
                with_close = sb_take(&closeb);
                free(full);
                full = with_close;
            }
            bc_chunks_push(&chunks, full);
            bc_free(&chunk_body);
        }
        {
            char *p = prefix.s;
            free(p);
        }
    }

    free(carry_lang);
    free(carry_header);
    bc_free(&rem);

    if (chunks.n > 1) {
        size_t i;
        for (i = 0; i < chunks.n; i++) {
            char buf[64];
            bc_sb b;
            char *s;
            snprintf(buf, sizeof(buf), " (%zu/%zu)", i + 1, chunks.n);
            sb_init(&b);
            sb_reserve(&b, strlen(chunks.v[i]) + 32);
            sb_add_str(&b, chunks.v[i]);
            sb_add_str(&b, buf);
            s = sb_take(&b);
            free(chunks.v[i]);
            chunks.v[i] = s;
        }
    }
    {
        size_t i;
        for (i = 0; i < chunks.n; i++) {
            json_append(out, json_string(chunks.v[i]));
            free(chunks.v[i]);
        }
        free(chunks.v);
    }
    return out;
}
