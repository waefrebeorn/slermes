/*
 * signal_format.c — C11 port of upstream Hermes
 * plugins/platforms/signal/signal_format.py (gate w26_signalform_c7c246).
 *
 * PoP: w26_signalform_c7c246_utf16_len @ plugins/platforms/signal/signal_format.py:_utf16_len
 * PoP: w26_signalform_c7c246_split_fenced @ plugins/platforms/signal/signal_format.py:_split_fenced
 * PoP: w26_signalform_c7c246_normalize_bullets @ plugins/platforms/signal/signal_format.py:_normalize_bullet_markers
 * PoP: w26_signalform_c7c246_markdown_to_signal @ plugins/platforms/signal/signal_format.py:markdown_to_signal
 * PoP: w26_signalform_c7c246_constants @ plugins/platforms/signal/signal_format.py:<module constants>
 *
 * Every regex in the source is re-implemented as a leftmost-first scanner over
 * code points (so every offset is the Python character offset), and the UTF-16
 * conversion happens only when a style range is finally printed.
 *
 * Seam: _fence_unfenced_tables() re-aligns a GFM table with wcwidth column
 * widths; that width table is not ported, so a table outside a fence returns
 * {"error": "table_seam"} rather than emitting mis-aligned columns.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_signalform_c7c246.h"

#define SF_MONO 2
#define SF_BOLD 0
#define SF_ITALIC 1
#define SF_STRIKE 3

/* Alphabetical on purpose: Python sorts the (start, len, style) tuples by name. */
static const char *const SF_STYLE_NAMES[4] = {"BOLD", "ITALIC", "MONOSPACE", "STRIKETHROUGH"};

/* _TABLE_WIDTH */
#define SF_TABLE_WIDTH W26_SIGNALFORM_C7C246_TABLE_WIDTH

/* ---- code-point buffers ---- */

typedef struct {
    int32_t *v;
    size_t n, cap;
} sbuf;

typedef struct {
    long start, len;
    int style;
} span;

typedef struct {
    span *v;
    size_t n, cap;
} spanlist;

typedef struct {
    size_t s, e;
} crange;

typedef struct {
    crange *v;
    size_t n, cap;
} rangelist;

static void *sf_grow(void *p, size_t *cap, size_t need, size_t elem) {
    if (need > *cap) {
        size_t nc = *cap ? *cap : 8;
        void *np;
        while (nc < need) nc *= 2;
        np = realloc(p, nc * elem);
        if (!np) return p;
        p = np;
        *cap = nc;
    }
    return p;
}

static void sb_init(sbuf *b) { b->v = NULL; b->n = 0; b->cap = 0; }
static void sb_free(sbuf *b) { free(b->v); b->v = NULL; b->n = b->cap = 0; }

static void sb_push(sbuf *b, int32_t c) {
    b->v = (int32_t *)sf_grow(b->v, &b->cap, b->n + 1, sizeof(int32_t));
    if (b->v) b->v[b->n++] = c;
}

static void sb_add(sbuf *b, const int32_t *p, size_t n) {
    if (!n || !p) return;
    b->v = (int32_t *)sf_grow(b->v, &b->cap, b->n + n, sizeof(int32_t));
    if (b->v) { memcpy(b->v + b->n, p, n * sizeof(int32_t)); b->n += n; }
}

static void sp_init(spanlist *l) { l->v = NULL; l->n = 0; l->cap = 0; }
static void sp_push(spanlist *l, long start, long len, int style) {
    l->v = (span *)sf_grow(l->v, &l->cap, l->n + 1, sizeof(span));
    if (l->v) { l->v[l->n].start = start; l->v[l->n].len = len; l->v[l->n].style = style; l->n++; }
}
static void sp_free(spanlist *l) { free(l->v); l->v = NULL; l->n = l->cap = 0; }

static void rl_init(rangelist *l) { l->v = NULL; l->n = 0; l->cap = 0; }
static void rl_push(rangelist *l, size_t s, size_t e) {
    l->v = (crange *)sf_grow(l->v, &l->cap, l->n + 1, sizeof(crange));
    if (l->v) { l->v[l->n].s = s; l->v[l->n].e = e; l->n++; }
}
static void rl_free(rangelist *l) { free(l->v); l->v = NULL; l->n = l->cap = 0; }

/* ---- UTF-8 <-> code points ---- */

static sbuf sf_decode(const char *s) {
    sbuf b;
    const unsigned char *u = (const unsigned char *)(s ? s : "");
    size_t i = 0;
    sb_init(&b);
    while (u[i]) {
        uint32_t c = u[i];
        if (c < 0x80) { sb_push(&b, (int32_t)c); i += 1; }
        else if ((c & 0xE0) == 0xC0 && u[i + 1]) {
            sb_push(&b, (int32_t)(((c & 0x1Fu) << 6) | (u[i + 1] & 0x3Fu))); i += 2;
        } else if ((c & 0xF0) == 0xE0 && u[i + 1] && u[i + 2]) {
            sb_push(&b, (int32_t)(((c & 0x0Fu) << 12) | ((u[i + 1] & 0x3Fu) << 6) | (u[i + 2] & 0x3Fu))); i += 3;
        } else if ((c & 0xF8) == 0xF0 && u[i + 1] && u[i + 2] && u[i + 3]) {
            sb_push(&b, (int32_t)(((c & 0x07u) << 18) | ((u[i + 1] & 0x3Fu) << 12) |
                                  ((u[i + 2] & 0x3Fu) << 6) | (u[i + 3] & 0x3Fu))); i += 4;
        } else { sb_push(&b, (int32_t)c); i += 1; }
    }
    return b;
}

static char *sf_encode(const int32_t *v, size_t n) {
    char *out = (char *)malloc(n * 4 + 1);
    size_t i, o = 0;
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        uint32_t c = (uint32_t)v[i];
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) {
            out[o++] = (char)(0xC0 | (c >> 6));
            out[o++] = (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out[o++] = (char)(0xE0 | (c >> 12));
            out[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (c & 0x3F));
        } else {
            out[o++] = (char)(0xF0 | (c >> 18));
            out[o++] = (char)(0x80 | ((c >> 12) & 0x3F));
            out[o++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    out[o] = '\0';
    return out;
}

static json_t *sf_string_of(const sbuf *b) {
    char *s = sf_encode(b->v, b->n);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* ---- character classes ---- */

static int sf_space(int32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int sf_word(int32_t c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '_' || c > 127;
}

static int sf_info_char(int32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '+' || c == '-';
}

static size_t sf_run_at(const int32_t *t, size_t n, size_t s) {
    size_t i = s;
    while (i < n && t[i] == '`') i++;
    return i - s;
}

/* The closing run of exactly `run` backticks, lazily scanning from `from`. */
static int sf_find_closing(const int32_t *t, size_t n, size_t from, size_t run, size_t *k_out) {
    size_t k;
    for (k = from; k + run <= n; k++) {
        size_t m;
        if (t[k] != '`') continue;
        for (m = 0; m < run; m++) if (t[k + m] != '`') break;
        if (m == run && (k + run == n || t[k + run] != '`')) { *k_out = k; return 1; }
    }
    return 0;
}

/* _split_fenced: alternating prose / fenced ranges over the source. */
static void sf_split_ranges(const int32_t *t, size_t n, rangelist *out) {
    size_t last = 0, s = 0;
    while (s < n) {
        size_t run, k;
        if (t[s] != '`') { s++; continue; }
        run = sf_run_at(t, n, s);
        if (run < 3) { s++; continue; }
        if (sf_find_closing(t, n, s + run, run, &k)) {
            rl_push(out, last, s);
            rl_push(out, s, k + run);
            last = k + run;
            s = last;
            continue;
        }
        s++;
    }
    rl_push(out, last, n);
}

json_t *w26_signalform_c7c246_split_fenced(const json_t *source) {
    const char *s = (source && source->type == JSON_STRING && source->str_val) ? source->str_val : "";
    sbuf src = sf_decode(s);
    rangelist parts;
    json_t *out = json_array();
    size_t i;
    rl_init(&parts);
    sf_split_ranges(src.v, src.n, &parts);
    for (i = 0; i < parts.n; i++) {
        sbuf part;
        sb_init(&part);
        sb_add(&part, src.v + parts.v[i].s, parts.v[i].e - parts.v[i].s);
        json_append(out, sf_string_of(&part));
        sb_free(&part);
    }
    rl_free(&parts);
    sb_free(&src);
    return out;
}

/* ---- bullet normalisation ---- */

static void sf_bullets_part(const int32_t *t, size_t n, sbuf *out) {
    size_t i = 0;
    while (i < n) {
        if (i == 0 || t[i - 1] == '\n') {
            size_t j = i, indent = 0;
            while (j < n && indent < 3 && (t[j] == ' ' || t[j] == '\t')) { j++; indent++; }
            if (j < n && (t[j] == '-' || t[j] == '*' || t[j] == '+')) {
                size_t k = j + 1;
                if (k < n && sf_space(t[k])) {
                    while (k < n && sf_space(t[k])) k++;
                    sb_add(out, t + i, j - i);
                    sb_push(out, 0x2022); /* • */
                    sb_push(out, ' ');
                    i = k;
                    continue;
                }
            }
        }
        sb_push(out, t[i]);
        i++;
    }
}

static void sf_normalize(const int32_t *t, size_t n, sbuf *out) {
    rangelist parts;
    size_t i;
    rl_init(&parts);
    sf_split_ranges(t, n, &parts);
    for (i = 0; i < parts.n; i++) {
        if (i % 2)
            sb_add(out, t + parts.v[i].s, parts.v[i].e - parts.v[i].s);
        else
            sf_bullets_part(t + parts.v[i].s, parts.v[i].e - parts.v[i].s, out);
    }
    rl_free(&parts);
}

json_t *w26_signalform_c7c246_normalize_bullets(const json_t *source) {
    const char *s = (source && source->type == JSON_STRING && source->str_val) ? source->str_val : "";
    sbuf src = sf_decode(s);
    sbuf out;
    json_t *res;
    sb_init(&out);
    sf_normalize(src.v, src.n, &out);
    res = sf_string_of(&out);
    sb_free(&out);
    sb_free(&src);
    return res;
}

json_t *w26_signalform_c7c246_utf16_len(const json_t *text) {
    const char *s = (text && text->type == JSON_STRING && text->str_val) ? text->str_val : "";
    sbuf src = sf_decode(s);
    size_t i, units = 0;
    for (i = 0; i < src.n; i++) units += (src.v[i] > 0xFFFF) ? 2 : 1;
    sb_free(&src);
    return json_number((double)units);
}

/* ---- tables: detection only (the wcwidth realignment is a documented seam) ---- */

static int sf_divider_cell(const int32_t *t, size_t s, size_t e) {
    size_t a = s, b = e, dashes = 0;
    while (a < b && (t[a] == ' ' || t[a] == '\t')) a++;
    while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\t')) b--;
    if (a < b && t[a] == ':') a++;
    if (b > a && t[b - 1] == ':') b--;
    while (a + dashes < b && t[a + dashes] == '-') dashes++;
    return dashes >= 3 && a + dashes == b;
}

static int sf_is_divider(const int32_t *t, size_t s, size_t e) {
    size_t i, cells = 0, cur = s;
    int have_pipe = 0;
    for (i = s; i < e; i++) if (t[i] == '|') { have_pipe = 1; break; }
    if (!have_pipe) return 0;
    for (i = s; i <= e; i++) {
        if (i == e || t[i] == '|') {
            size_t a = cur, b = i;
            while (a < b && (t[a] == ' ' || t[a] == '\t')) a++;
            while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\t')) b--;
            if (b > a) {
                cells++;
                if (!sf_divider_cell(t, cur, i)) return 0;
            }
            cur = i + 1;
        }
    }
    return cells > 1;
}

/* A header row (with a pipe) directly above a divider row is a GFM table. */
static int sf_has_table(const int32_t *t, size_t n) {
    size_t line = 0;
    while (line <= n) {
        size_t e = line, ls, le;
        int pipe = 0;
        size_t i;
        while (e < n && t[e] != '\n') e++;
        for (i = line; i < e; i++) if (t[i] == '|') { pipe = 1; break; }
        if (pipe && e < n) {
            ls = e + 1;
            le = ls;
            while (le < n && t[le] != '\n') le++;
            if (sf_is_divider(t, ls, le)) return 1;
        }
        if (e >= n) break;
        line = e + 1;
    }
    return 0;
}

static int sf_any_table(const int32_t *t, size_t n) {
    rangelist parts;
    size_t i;
    int found = 0;
    rl_init(&parts);
    sf_split_ranges(t, n, &parts);
    for (i = 0; i < parts.n && !found; i++)
        if (!(i % 2)) found = sf_has_table(t + parts.v[i].s, parts.v[i].e - parts.v[i].s);
    rl_free(&parts);
    return found;
}

/* ---- code fences -> MONOSPACE ---- */

static int sf_find_code_block(const int32_t *t, size_t n, size_t from,
                              size_t *ms, size_t *me, size_t *g2s, size_t *g2e) {
    size_t s;
    for (s = from; s < n; s++) {
        size_t run, j, k;
        if (t[s] != '`') continue;
        run = sf_run_at(t, n, s);
        if (run < 3) continue;
        j = s + run;
        while (j < n && sf_info_char(t[j])) j++;
        if (j < n && t[j] == '\n') j++;
        if (sf_find_closing(t, n, j, run, &k)) {
            *ms = s; *me = k + run; *g2s = j; *g2e = k;
            return 1;
        }
    }
    return 0;
}

/* ---- inline patterns ---- */

typedef struct {
    size_t ms, me, g1s, g1e;
    int style;
} imatch;

typedef struct {
    imatch *v;
    size_t n, cap;
} imatchlist;

static void im_push(imatchlist *l, imatch m) {
    l->v = (imatch *)sf_grow(l->v, &l->cap, l->n + 1, sizeof(imatch));
    if (l->v) l->v[l->n++] = m;
}

/* One attempt at position i; the search slice is [gs, ge), lookbehind sees t[0..]. */
static int sf_try_inline(const int32_t *t, int style, size_t i, size_t ge, imatch *out) {
    size_t k;
    if (i >= ge) return 0;
    if (style == SF_MONO) {                       /* `(.+?)` */
        if (t[i] != '`') return 0;
        for (k = i + 2; k < ge; k++)
            if (t[k] == '`') {
                out->ms = i; out->me = k + 1; out->g1s = i + 1; out->g1e = k; out->style = style;
                return 1;
            }
        return 0;
    }
    if (style == SF_BOLD) {                       /* **(.+?)** / __(.+?)__ */
        int32_t mark = t[i];
        if (mark != '*' && mark != '_') return 0;
        if (i + 1 >= ge || t[i + 1] != mark) return 0;
        for (k = i + 3; k + 1 < ge; k++)
            if (t[k] == mark && t[k + 1] == mark) {
                out->ms = i; out->me = k + 2; out->g1s = i + 2; out->g1e = k; out->style = style;
                return 1;
            }
        return 0;
    }
    if (style == SF_STRIKE) {                     /* ~~(.+?)~~ */
        if (t[i] != '~' || i + 1 >= ge || t[i + 1] != '~') return 0;
        for (k = i + 3; k + 1 < ge; k++)
            if (t[k] == '~' && t[k + 1] == '~') {
                out->ms = i; out->me = k + 2; out->g1s = i + 2; out->g1e = k; out->style = style;
                return 1;
            }
        return 0;
    }
    return 0;
}

/* Italic star: (?<!\*)\*(?!\*| )(.+?)(?<!\*)\*(?!\*) */
static int sf_try_italic_star(const int32_t *t, size_t i, int prev_is_star, size_t ge, imatch *out) {
    size_t k;
    if (t[i] != '*' || prev_is_star) return 0;
    if (i + 1 >= ge) return 0;
    if (t[i + 1] == '*' || t[i + 1] == ' ') return 0;
    for (k = i + 2; k < ge; k++) {
        if (t[k] != '*') continue;
        if (t[k - 1] == '*') continue;
        if (k + 1 < ge && t[k + 1] == '*') continue;
        out->ms = i; out->me = k + 1; out->g1s = i + 1; out->g1e = k; out->style = SF_ITALIC;
        return 1;
    }
    return 0;
}

/* Italic underscore: (?<!\w)_(?!_)(.+?)(?<!_)_(?!\w) */
static int sf_try_italic_under(const int32_t *t, size_t i, int prev_is_word, size_t ge, imatch *out) {
    size_t k;
    if (t[i] != '_' || prev_is_word) return 0;
    if (i + 1 >= ge) return 0;
    if (t[i + 1] == '_') return 0;
    for (k = i + 2; k < ge; k++) {
        if (t[k] != '_') continue;
        if (t[k - 1] == '_') continue;
        if (k + 1 < ge && sf_word(t[k + 1])) continue;
        out->ms = i; out->me = k + 1; out->g1s = i + 1; out->g1e = k; out->style = SF_ITALIC;
        return 1;
    }
    return 0;
}

/* One gap for one pattern id; ids order the six patterns of _INLINE_PATTERNS. */
static void sf_scan_gap(const int32_t *t, int pid, size_t gs, size_t ge, imatchlist *out) {
    size_t i = gs;
    while (i < ge) {
        imatch m;
        int hit = 0;
        switch (pid) {
        case 0: hit = sf_try_inline(t, SF_MONO, i, ge, &m); break;
        case 1: hit = t[i] == '*' && sf_try_inline(t, SF_BOLD, i, ge, &m); break;
        case 2: hit = t[i] == '_' && sf_try_inline(t, SF_BOLD, i, ge, &m); break;
        case 3: hit = sf_try_inline(t, SF_STRIKE, i, ge, &m); break;
        case 4: hit = sf_try_italic_star(t, i, i > 0 && t[i - 1] == '*', ge, &m); break;
        case 5: hit = sf_try_italic_under(t, i, i > 0 && sf_word(t[i - 1]), ge, &m); break;
        default: break;
        }
        if (hit) {
            im_push(out, m);
            i = m.me > m.ms ? m.me : m.ms + 1;
        } else {
            i++;
        }
    }
}

/* ---- ordering helpers ---- */

static int sf_imatch_cmp(const void *a, const void *b) {
    const imatch *x = (const imatch *)a, *y = (const imatch *)b;
    if (x->ms != y->ms) return x->ms < y->ms ? -1 : 1;
    if (x->me != y->me) return x->me < y->me ? -1 : 1;
    if (x->g1s != y->g1s) return x->g1s < y->g1s ? -1 : 1;
    if (x->g1e != y->g1e) return x->g1e < y->g1e ? -1 : 1;
    if (x->style != y->style) return x->style < y->style ? -1 : 1;
    return 0;
}

static int sf_span_cmp(const void *a, const void *b) {
    const span *x = (const span *)a, *y = (const span *)b;
    if (x->start != y->start) return x->start < y->start ? -1 : 1;
    if (x->len != y->len) return x->len < y->len ? -1 : 1;
    if (x->style != y->style) return x->style < y->style ? -1 : 1;
    return 0;
}

static size_t sf_shift(size_t pos, const rangelist *rem) {
    size_t shift = 0, m;
    for (m = 0; m < rem->n; m++) {
        size_t rp = rem->v[m].s, rl = rem->v[m].e - rem->v[m].s;
        size_t adv;
        if (rp >= pos) break;
        adv = (pos - rp) < rl ? (pos - rp) : rl;
        shift += adv;
    }
    return pos - shift;
}

/* ---- the full markdown -> (text, styles) pass ---- */

static void sf_collapse_strip(const int32_t *t, size_t n, sbuf *out) {
    sbuf tmp;
    size_t i = 0, s, e;
    sb_init(&tmp);
    while (i < n) {
        if (t[i] == '\n') {
            size_t j = i;
            while (j < n && t[j] == '\n') j++;
            sb_push(&tmp, '\n');
            if (j - i >= 2) sb_push(&tmp, '\n'); /* \n{3,} -> \n\n */
            i = j;
        } else {
            sb_push(&tmp, t[i]);
            i++;
        }
    }
    s = 0;
    e = tmp.n;
    while (s < e && sf_space(tmp.v[s])) s++;
    while (e > s && sf_space(tmp.v[e - 1])) e--;
    sb_add(out, tmp.v ? tmp.v + s : NULL, e - s);
    sb_free(&tmp);
}

json_t *w26_signalform_c7c246_markdown_to_signal(const json_t *textv) {
    const char *in = (textv && textv->type == JSON_STRING && textv->str_val) ? textv->str_val : "";
    sbuf src = sf_decode(in), t1, t2, unw, htext, hfinal, result;
    spanlist styles, hstyles, inline_styles, prior;
    rangelist blocks, hrem, rem;
    size_t i, last, last_end;
    imatchlist allm;
    json_t *obj, *arr;

    sb_init(&t1); sb_init(&t2); sb_init(&unw); sb_init(&htext); sb_init(&hfinal); sb_init(&result);
    sp_init(&styles); sp_init(&hstyles); sp_init(&inline_styles); sp_init(&prior);
    rl_init(&blocks); rl_init(&hrem); rl_init(&rem);
    allm.v = NULL; allm.n = allm.cap = 0;

    sf_collapse_strip(src.v, src.n, &t1);
    sf_normalize(t1.v, t1.n, &t2);

    if (sf_any_table(t2.v, t2.n)) {
        obj = json_object();
        json_set(obj, "error", json_string("table_seam"));
        sb_free(&src); sb_free(&t1); sb_free(&t2);
        return obj;
    }

    /* code fences: styles + blocks are recorded against the unwrapped text */
    last = 0;
    {
        size_t ms, me, g2s, g2e;
        while (sf_find_code_block(t2.v, t2.n, last, &ms, &me, &g2s, &g2e)) {
            size_t g2e2 = g2e;
            while (g2e2 > g2s && t2.v[g2e2 - 1] == '\n') g2e2--;
            sb_add(&unw, t2.v + last, ms - last);
            sp_push(&styles, (long)unw.n, (long)(g2e2 - g2s), SF_MONO);
            rl_push(&blocks, unw.n, unw.n + (g2e2 - g2s));
            sb_add(&unw, t2.v + g2s, g2e2 - g2s);
            last = me;
        }
    }
    sb_add(&unw, t2.v + last, t2.n - last);

    /* headings: strip the marker run, style the line BOLD */
    last_end = 0;
    for (i = 0; i < unw.n; i++) {
        size_t r, me2, eol, b;
        int in_block = 0;
        if (unw.v[i] != '#') continue;
        if (i != 0 && unw.v[i - 1] != '\n') continue;
        r = 0;
        while (i + r < unw.n && unw.v[i + r] == '#') r++;
        if (r > 6) continue;
        if (i + r >= unw.n || !sf_space(unw.v[i + r])) continue;
        me2 = i + r;
        while (me2 < unw.n && sf_space(unw.v[me2])) me2++;
        for (b = 0; b < blocks.n; b++)
            if (blocks.v[b].s <= i && i < blocks.v[b].e) { in_block = 1; break; }
        if (in_block) continue;
        eol = me2;
        while (eol < unw.n && unw.v[eol] != '\n') eol++;
        sb_add(&htext, unw.v + last_end, i - last_end);
        sp_push(&hstyles, (long)htext.n, (long)(eol - me2), SF_BOLD);
        sb_add(&htext, unw.v + me2, eol - me2);
        rl_push(&hrem, i, me2);
        last_end = eol;
        i = eol - 1;
    }

    if (hrem.n) {
        sb_add(&hfinal, htext.v, htext.n);
        sb_add(&hfinal, unw.v + last_end, unw.n - last_end);
        /* blocks shift with the removed heading markers */
        for (i = 0; i < blocks.n; i++) {
            size_t s0 = sf_shift(blocks.v[i].s, &hrem);
            size_t e0 = sf_shift(blocks.v[i].e, &hrem);
            blocks.v[i].s = s0;
            blocks.v[i].e = e0;
        }
        {
            spanlist adj;
            sp_init(&adj);
            for (i = 0; i < styles.n; i++) {
                size_t s0 = sf_shift((size_t)styles.v[i].start, &hrem);
                size_t e0 = sf_shift((size_t)(styles.v[i].start + styles.v[i].len), &hrem);
                if (e0 > s0) sp_push(&adj, (long)s0, (long)(e0 - s0), styles.v[i].style);
            }
            sp_free(&styles);
            styles = adj;
            for (i = 0; i < hstyles.n; i++)
                sp_push(&styles, hstyles.v[i].start, hstyles.v[i].len, hstyles.v[i].style);
        }
    } else {
        sb_add(&hfinal, unw.v, unw.n);
    }

    /* inline patterns over the gaps between code blocks */
    {
        rangelist occ;
        int pid;
        rl_init(&occ);
        for (i = 0; i < blocks.n; i++) rl_push(&occ, blocks.v[i].s, blocks.v[i].e);

        for (pid = 0; pid < 6; pid++) {
            imatchlist found;
            size_t b, count;
            found.v = NULL; found.n = found.cap = 0;
            count = blocks.n + 1;
            for (b = 0; b < count; b++) {
                size_t gs = b == 0 ? 0 : blocks.v[b - 1].e;
                size_t ge = b == blocks.n ? hfinal.n : blocks.v[b].s;
                if (gs < ge) sf_scan_gap(hfinal.v, pid, gs, ge, &found);
            }
            for (i = 0; i < found.n; i++) {
                size_t o, overlap = 0;
                for (o = 0; o < occ.n; o++)
                    if (found.v[i].ms < occ.v[o].e && found.v[i].me > occ.v[o].s) { overlap = 1; break; }
                if (overlap) continue;
                im_push(&allm, found.v[i]);
                rl_push(&occ, found.v[i].ms, found.v[i].me);
            }
            free(found.v);
        }
        rl_free(&occ);
    }

    if (allm.n > 1) qsort(allm.v, allm.n, sizeof(imatch), sf_imatch_cmp);

    /* strip the markers, capturing the inline ranges in the stripped text */
    last_end = 0;
    for (i = 0; i < allm.n; i++) {
        size_t ms = allm.v[i].ms, me = allm.v[i].me, g1s = allm.v[i].g1s, g1e = allm.v[i].g1e;
        if (ms < last_end) continue;
        if (g1s > ms) rl_push(&rem, ms, g1s);
        if (me > g1e) rl_push(&rem, g1e, me);
        sb_add(&result, hfinal.v + last_end, ms - last_end);
        sp_push(&inline_styles, (long)result.n, (long)(g1e - g1s), allm.v[i].style);
        sb_add(&result, hfinal.v + g1s, g1e - g1s);
        last_end = me;
    }
    sb_add(&result, hfinal.v + last_end, hfinal.n - last_end);

    /* removals sorted by start position for the shift scan */
    {
        size_t a, b2;
        for (a = 0; a + 1 < rem.n; a++)
            for (b2 = a + 1; b2 < rem.n; b2++)
                if (rem.v[b2].s < rem.v[a].s) { crange tmp = rem.v[a]; rem.v[a] = rem.v[b2]; rem.v[b2] = tmp; }
    }

    /* prior spans adjusted past the removed markers */
    for (i = 0; i < styles.n; i++) {
        size_t s0 = sf_shift((size_t)styles.v[i].start, &rem);
        size_t e0 = sf_shift((size_t)(styles.v[i].start + styles.v[i].len), &rem);
        if (e0 > s0) sp_push(&prior, (long)s0, (long)(e0 - s0), styles.v[i].style);
    }
    for (i = 0; i < inline_styles.n; i++)
        sp_push(&prior, inline_styles.v[i].start, inline_styles.v[i].len, inline_styles.v[i].style);
    if (prior.n > 1) qsort(prior.v, prior.n, sizeof(span), sf_span_cmp);

    obj = json_object();
    arr = json_array();
    for (i = 0; i < prior.n; i++) {
        long start = prior.v[i].start, len = prior.v[i].len;
        size_t p, units_s = 0, units_l = 0;
        char label[128];
        if (start < 0 || start + len > (long)result.n) continue;
        for (p = 0; p < (size_t)start; p++) units_s += result.v[p] > 0xFFFF ? 2 : 1;
        for (p = (size_t)start; p < (size_t)(start + len); p++) units_l += result.v[p] > 0xFFFF ? 2 : 1;
        snprintf(label, sizeof(label), "%zu:%zu:%s", units_s, units_l, SF_STYLE_NAMES[prior.v[i].style]);
        json_append(arr, json_string(label));
    }
    {
        char *out = sf_encode(result.v, result.n);
        json_set(obj, "text", json_string(out ? out : ""));
        free(out);
    }
    json_set(obj, "styles", arr);

    sb_free(&src); sb_free(&t1); sb_free(&t2); sb_free(&unw); sb_free(&htext);
    sb_free(&hfinal); sb_free(&result);
    sp_free(&styles); sp_free(&hstyles); sp_free(&inline_styles); sp_free(&prior);
    rl_free(&blocks); rl_free(&hrem); rl_free(&rem);
    free(allm.v);
    return obj;
}

json_t *w26_signalform_c7c246_constants(void) {
    static const char *const patterns[6][2] = {
        {"`(.+?)`", "MONOSPACE"},
        {"\\*\\*(.+?)\\*\\*", "BOLD"},
        {"__(.+?)__", "BOLD"},
        {"~~(.+?)~~", "STRIKETHROUGH"},
        {"(?<!\\*)\\*(?!\\*| )(.+?)(?<!\\*)\\*(?!\\*)", "ITALIC"},
        {"(?<!\\w)_(?!_)(.+?)(?<!_)_(?!\\w)", "ITALIC"},
    };
    json_t *o = json_object();
    json_t *styles = json_array();
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < 4; i++) json_append(styles, json_string(SF_STYLE_NAMES[i]));
    for (i = 0; i < 6; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(patterns[i][0]));
        json_append(pair, json_string(patterns[i][1]));
        json_append(arr, pair);
    }
    json_set(o, "table_width", json_number((double)SF_TABLE_WIDTH));
    json_set(o, "styles", styles);
    json_set(o, "inline_patterns", arr);
    json_set(o, "code_block_pattern", json_string("(`{3,})[a-zA-Z0-9_+-]*\\n?(.*?)\\1(?!`)"));
    json_set(o, "fence_split_pattern", json_string("((`{3,}).*?\\2(?!`))"));
    json_set(o, "heading_pattern", json_string("^#{1,6}\\s+"));
    return o;
}
