/*
 * telegram_markdown.c - C11 port of upstream Hermes
 * plugins/platforms/telegram/telegram_markdown.py.
 *
 * Gates: w31_telegramma_02bbc8_p1 (the MarkdownV2 conversion),
 *        w31_telegramma_02bbc8_p2 (its pure text primitives).
 *
 * PoP: w31_telegramma_02bbc8_p1_to_markdown_v2 @ plugins/platforms/telegram/telegram_markdown.py:to_markdown_v2
 * PoP: w31_telegramma_02bbc8_p2_escape_mdv2 @ plugins/platforms/telegram/telegram_markdown.py:_escape_mdv2
 * PoP: w31_telegramma_02bbc8_p2_url_end @ plugins/platforms/telegram/telegram_markdown.py:_url_end
 * PoP: w31_telegramma_02bbc8_p2_link_target_renders @ plugins/platforms/telegram/telegram_markdown.py:link_target_renders
 * PoP: w31_telegramma_02bbc8_p2_crosses_quote_edge @ plugins/platforms/telegram/telegram_markdown.py:_crosses_quote_edge
 *
 * Internal 1:1 ports (the machinery to_markdown_v2 is made of):
 *   _protect_fenced, _stash_inline_code, _MD_LINK_RE/_convert_link,
 *   _stash_bare_urls/_UrlEmphasisScan, _convert_header, expandable-quote
 *   sentinel, _resolve_emphasis/_pair_emphasis/_StarPairing/_pick_opener,
 *   ~~strike~~ and ||spoiler||, _convert_blockquote, _escape_mdv2,
 *   _expand_placeholders and the _escape_bare_bracket safety net.
 *
 * SEAMS (declared, not ported here - each already has its own C home and gate):
 *   * normalize_latex_macros    (plugins/platforms/telegram/telegram_latex.py)
 *   * convert_table_to_bullets  (gateway/platforms/helpers.py)
 * Both run inside step 0, are identity for text without a macro name or a pipe
 * table, and are pluggable through w31_telegramma_02bbc8_p1_set_seams().
 * The transport (sendMessage, flood control, retries) is the caller's.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "noc_w31_telegramma_02bbc8_p1.h"
#include "noc_w31_telegramma_02bbc8_p2.h"

/* ---------------------------------------------------------------- memory -- */

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) abort(); /* allocation failure is fatal; no growth loop */
    return q;
}

#define PUSH(ptr, n, cap, val)                                                  \
    do {                                                                        \
        if ((n) == (cap)) {                                                     \
            (cap) = (cap) ? (cap) * 2 : 8;                                      \
            (ptr) = xrealloc((ptr), sizeof(*(ptr)) * (size_t)(cap));            \
        }                                                                       \
        (ptr)[(n)++] = (val);                                                   \
    } while (0)

/* A Python str: bytes plus a length.  NUL bytes are data (placeholders use
 * them), so nothing here is NUL-terminated internally. */
typedef struct { char *p; size_t n; } str_t;

typedef struct { char *p; size_t n, cap; } sb_t;

static void sb_init(sb_t *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static void sb_addn(sb_t *b, const char *s, size_t n) {
    if (!n) return;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->n + n + 1) cap *= 2;
        b->p = xrealloc(b->p, cap);
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_adds(sb_t *b, const char *s) { sb_addn(b, s, strlen(s)); }
static void sb_addc(sb_t *b, char c) { sb_addn(b, &c, 1); }
static void sb_addstr(sb_t *b, str_t s) { sb_addn(b, s.p ? s.p : "", s.n); }

static str_t sb_take(sb_t *b) {
    str_t r;
    if (!b->p) { r.p = xrealloc(NULL, 1); r.p[0] = '\0'; r.n = 0; }
    else { r.p = b->p; r.n = b->n; }
    b->p = NULL; b->n = 0; b->cap = 0;
    return r;
}

static str_t str_copy(const char *s, size_t n) {
    str_t r;
    r.p = xrealloc(NULL, n + 1);
    if (n) memcpy(r.p, s, n);
    r.p[n] = '\0';
    r.n = n;
    return r;
}

static str_t str_from(const char *s) { return str_copy(s, strlen(s)); }

static str_t str_slice(str_t s, size_t a, size_t b) {
    if (b < a) b = a;
    if (b > s.n) b = s.n;
    if (a > s.n) a = s.n;
    return str_copy(s.p + a, b - a);
}

/* Index of *needle* in [from, n), or -1. */
static long find_bytes(const char *h, size_t n, size_t from, const char *needle, size_t nn) {
    if (!nn) return (long)(from <= n ? from : n);
    if (from > n || nn > n) return -1;
    for (size_t i = from; i + nn <= n; i++) {
        if (h[i] == needle[0] && memcmp(h + i, needle, nn) == 0) return (long)i;
    }
    return -1;
}

/* Last index of *needle* in [0, limit), or -1. */
static long rfind_bytes(const char *h, size_t limit, const char *needle, size_t nn) {
    if (!nn || limit < nn) return -1;
    for (size_t i = limit - nn + 1; i-- > 0;) {
        if (h[i] == needle[0] && memcmp(h + i, needle, nn) == 0) return (long)i;
    }
    return -1;
}

/* ----------------------------------------------------------------- utf-8 -- */

static int u_decode(const unsigned char *s, size_t n, size_t *len) {
    unsigned char c = s[0];
    if (c < 0x80) { *len = 1; return c; }
    if ((c & 0xE0) == 0xC0 && n >= 2) { *len = 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && n >= 3) {
        *len = 3;
        return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && n >= 4) {
        *len = 4;
        return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    *len = 1;
    return c;
}

/* Python's str.isspace() over one code point. */
static int cp_isspace(int cp) {
    if (cp >= 0x09 && cp <= 0x0D) return 1;
    if (cp >= 0x1C && cp <= 0x1F) return 1;
    if (cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    return 0;
}

/* Whitespace at byte offset pos (0 when pos is past the end). */
static int u_isspace_at(const char *s, size_t n, size_t pos) {
    size_t len = 0;
    unsigned char b;
    if (pos >= n) return 0;
    b = (unsigned char)s[pos];
    if ((b & 0xC0) == 0x80) return 0; /* a continuation byte is not a code point */
    return cp_isspace(u_decode((const unsigned char *)s + pos, n - pos, &len));
}

/* Whitespace of the code point that ends at byte offset pos. */
static int u_isspace_before(const char *s, size_t pos) {
    size_t start;
    size_t len = 0;
    if (pos == 0) return 0;
    start = pos - 1;
    while (start > 0 && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
    return cp_isspace(u_decode((const unsigned char *)s + start, pos - start, &len));
}

/* --------------------------------------------------------- placeholders -- */

typedef struct { str_t *v; int n, cap; } phstore_t;

/* f"\x00PH{counter}\x00" */
static str_t ph_stash(phstore_t *p, str_t val) {
    int idx = p->n;
    char num[16];
    sb_t k;
    PUSH(p->v, p->n, p->cap, val);
    sb_init(&k);
    sb_addc(&k, '\0');
    sb_adds(&k, "PH");
    snprintf(num, sizeof num, "%d", idx);
    sb_adds(&k, num);
    sb_addc(&k, '\0');
    return sb_take(&k);
}

/* _expand_placeholders: every \x00PH<n>\x00 whose index is below *limit*. */
static str_t expand_placeholders(str_t text, phstore_t *p, int limit) {
    sb_t out;
    size_t i = 0;
    if (!memchr(text.p, '\0', text.n)) return text;
    sb_init(&out);
    while (i < text.n) {
        if (text.p[i] == '\0' && i + 3 < text.n && text.p[i + 1] == 'P' && text.p[i + 2] == 'H') {
            size_t j = i + 3;
            long idx = 0;
            int digits = 0;
            while (j < text.n && text.p[j] >= '0' && text.p[j] <= '9' && digits < 9) {
                idx = idx * 10 + (text.p[j] - '0');
                j++;
                digits++;
            }
            if (digits && j < text.n && text.p[j] == '\0' && idx >= 0 && idx < limit && idx < p->n) {
                sb_addstr(&out, p->v[idx]);
                i = j + 1;
                continue;
            }
        }
        sb_addc(&out, text.p[i]);
        i++;
    }
    return sb_take(&out);
}

/* ------------------------------------------------- p2: escaping / links -- */

static int is_mdv2_special(char c) {
    return c == '_' || c == '*' || c == '[' || c == ']' || c == '(' || c == ')' ||
           c == '~' || c == '`' || c == '>' || c == '#' || c == '+' || c == '-' ||
           c == '=' || c == '|' || c == '{' || c == '}' || c == '.' || c == '!' ||
           c == '\\';
}

static str_t escape_mdv2_s(str_t t) {
    sb_t out;
    size_t i;
    sb_init(&out);
    for (i = 0; i < t.n; i++) {
        if (is_mdv2_special(t.p[i])) sb_addc(&out, '\\');
        sb_addc(&out, t.p[i]);
    }
    return sb_take(&out);
}

char *w31_telegramma_02bbc8_p2_escape_mdv2(const char *text) {
    str_t in = str_from(text ? text : "");
    str_t out = escape_mdv2_s(in);
    free(in.p);
    return out.p;
}

/* _URL_TAIL_CHARS = ".,;:!?'\"" */
static int is_url_tail_char(char c) {
    return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' ||
           c == '\'' || c == '"';
}

/* _url_end */
static size_t url_end_len(const char *u, size_t n) {
    size_t end = n;
    long unclosed = 0;
    for (size_t i = 0; i < n; i++) {
        if (u[i] == ')') unclosed++;
        else if (u[i] == '(') unclosed--;
    }
    while (end) {
        char ch = u[end - 1];
        if (ch == ')' && unclosed > 0) {
            unclosed--;
        } else if (ch == '(') {
            unclosed++;
            break;
        } else if (!is_url_tail_char(ch)) {
            break;
        }
        end--;
    }
    return end;
}

/* Length of the trimmed prefix in Python str units (code points), which is what
 * ``_url_end`` answers; the port's own callers use url_end_len (bytes). */
long w31_telegramma_02bbc8_p2_url_end(const char *url) {
    size_t n, b, i;
    long cps = 0;
    if (!url) return 0;
    n = strlen(url);
    b = url_end_len(url, n);
    for (i = 0; i < b; i++) {
        if (((unsigned char)url[i] & 0xC0) != 0x80) cps++;
    }
    return cps;
}

static int ascii_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int ascii_digit(char c) { return c >= '0' && c <= '9'; }
static int ascii_alnum(char c) { return ascii_alpha(c) || ascii_digit(c); }

/* Python's \b before pos: \w is str.isalnum() plus "_", so non-ASCII letters
 * and digits are word characters while Latin-1 punctuation, general
 * punctuation, dingbats, variation selectors and emoji are not (a documented
 * approximation of the Unicode categories Python consults). */
static int u_isword_before(const char *s, size_t pos) {
    size_t start;
    size_t len = 0;
    int cp;
    if (pos == 0) return 0;
    start = pos - 1;
    while (start > 0 && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
    cp = u_decode((const unsigned char *)s + start, pos - start, &len);
    if (cp < 0x80) return ascii_alnum((char)cp) || cp == '_';
    if (cp >= 0x80 && cp <= 0xBF) return 0;
    if (cp >= 0x2000 && cp <= 0x2BFF) return 0;
    if (cp >= 0x3000 && cp <= 0x303F) return 0;
    if (cp >= 0xFE00 && cp <= 0xFE0F) return 0;
    if (cp >= 0x1F000 && cp <= 0x1FAFF) return 0;
    return 1;
}


/* _RENDERABLE_LINK_TARGET_RE over a stripped target. */
static int target_renders_raw(const char *s, size_t n) {
    size_t i;
    if (!n) return 0;
    /* Branch A: scheme URL - [a-z][a-z0-9+.-]*: then [^\s@]\S* */
    if (ascii_alpha(s[0])) {
        i = 1;
        while (i < n && (ascii_alnum(s[i]) || s[i] == '+' || s[i] == '.' || s[i] == '-')) i++;
        if (i < n && s[i] == ':') {
            size_t j;
            i++;
            if (i < n && !u_isspace_at(s, n, i) && s[i] != '@') {
                for (j = i; j < n; j++) {
                    if (u_isspace_at(s, n, j)) break;
                }
                if (j == n) return 1;
            }
        }
    }
    /* Branch B: bare domain - [a-z0-9-]+(\.[a-z0-9-]+)+([/?#:]\S*)? */
    i = 0;
    while (i < n && (ascii_alnum(s[i]) || s[i] == '-')) i++;
    if (i > 0) {
        int groups = 0;
        while (i < n && s[i] == '.') {
            size_t j = i + 1;
            size_t k = j;
            while (k < n && (ascii_alnum(s[k]) || s[k] == '-')) k++;
            if (k == j) break;
            groups++;
            i = k;
        }
        if (groups > 0) {
            if (i == n) return 1;
            if (s[i] == '/' || s[i] == '?' || s[i] == '#' || s[i] == ':') {
                size_t j;
                for (j = i + 1; j < n; j++) {
                    if (u_isspace_at(s, n, j)) break;
                }
                if (j == n) return 1;
            }
        }
    }
    return 0;
}

int w31_telegramma_02bbc8_p2_link_target_renders(const char *target) {
    size_t n, a, b;
    if (!target) return 0;
    n = strlen(target);
    a = 0;
    b = n;
    while (a < b && u_isspace_at(target, n, a)) a++;
    while (b > a && u_isspace_before(target, b)) b--;
    return target_renders_raw(target + a, b - a);
}

/* ----------------------------------------------------------- quote edges -- */

static const char EQ_SENTINEL[] = "\x00" "EQ" "\x00"; /* _EXPANDABLE_QUOTE */
#define EQ_LEN 4

static int line_begins_quote(const char *t, size_t n, size_t start) {
    if (start >= n) return 0;
    if (n - start >= EQ_LEN + 1 && memcmp(t + start, EQ_SENTINEL, EQ_LEN) == 0 &&
        t[start + EQ_LEN] == '>')
        return 1;
    return t[start] == '>';
}

static int bordering_quote_newline(const char *t, size_t n, size_t pos, size_t line_start) {
    return line_begins_quote(t, n, line_start) || line_begins_quote(t, n, pos + 1);
}

static int crosses_quote_edge_raw(const char *t, size_t n, size_t start, size_t end) {
    long pos;
    size_t line_start;
    long ls;
    if (end > n) end = n;
    if (start > end) return 0;
    pos = find_bytes(t, n, start, "\n", 1);
    if (pos < 0 || (size_t)pos >= end) return 0;
    ls = rfind_bytes(t, start, "\n", 1);
    line_start = ls < 0 ? 0 : (size_t)ls + 1;
    while (pos >= 0 && (size_t)pos < end) {
        if (bordering_quote_newline(t, n, (size_t)pos, line_start)) return 1;
        line_start = (size_t)pos + 1;
        pos = find_bytes(t, n, line_start, "\n", 1);
    }
    return 0;
}

int w31_telegramma_02bbc8_p2_crosses_quote_edge(const char *text, long start, long end) {
    size_t n;
    if (!text) return 0;
    n = strlen(text);
    if (start < 0) start = 0;
    if (end < 0) return 0;
    return crosses_quote_edge_raw(text, n, (size_t)start, (size_t)end);
}

/* -------------------------------------------------------- span edges ----- */

/* _SpanEdges */
typedef struct {
    int *nl; int nnl, cnl;
    int *blank; int nblank, cblank;
    int *qedge; int nq, cq;
} edges_t;

static int bisect_left(const int *a, int n, int x) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static void edges_init(edges_t *e, const char *t, size_t n) {
    size_t i, line_start = 0;
    memset(e, 0, sizeof *e);
    for (i = 0; i < n; i++) {
        if (t[i] == '\n') PUSH(e->nl, e->nnl, e->cnl, (int)i);
    }
    for (i = 0; i < (size_t)e->nnl; i++) {
        int p = e->nl[i];
        if ((size_t)p + 1 < n && t[p + 1] == '\n') PUSH(e->blank, e->nblank, e->cblank, p);
    }
    for (i = 0; i < (size_t)e->nnl; i++) {
        int pos = e->nl[i];
        if (bordering_quote_newline(t, n, (size_t)pos, line_start))
            PUSH(e->qedge, e->nq, e->cq, pos);
        line_start = (size_t)pos + 1;
    }
}

static void edges_free(edges_t *e) {
    free(e->nl); free(e->blank); free(e->qedge);
    memset(e, 0, sizeof *e);
}

static int edges_has_newline(const edges_t *e, int start, int end) {
    int i = bisect_left(e->nl, e->nnl, start);
    return i < e->nnl && e->nl[i] < end;
}

static int edges_has_blank_line(const edges_t *e, int start, int end) {
    int i = bisect_left(e->blank, e->nblank, start);
    return i < e->nblank && e->blank[i] + 2 <= end;
}

static int edges_crosses_quote_edge(const edges_t *e, int start, int end) {
    int i = bisect_left(e->qedge, e->nq, start);
    return i < e->nq && e->qedge[i] < end;
}

/* ------------------------------------------------------- star pairing ----- */

enum { K_BOLD = 0, K_ITALIC = 1 };

typedef struct { int kind; int partner; } pair_t;

typedef struct {
    int start, end;
    int can_open, can_close;
    int remaining;
    pair_t *opens; int nopens, copens;
    pair_t *closes; int ncloses, ccloses;
} dtok_t;

static void dtok_free(dtok_t *d) { free(d->opens); free(d->closes); }

typedef struct { int kind; int at; } cl_t;
typedef struct { cl_t *v; int n, cap; } plist_t;

static void plist_free(plist_t *p) { free(p->v); p->v = NULL; p->n = p->cap = 0; }

typedef struct {
    dtok_t *t; int n, cap;
    int *stack; int ns, cs;
    int *near2; int c2, c2cap;
    int bottoms[3][2];
    edges_t edges;
    int owns_edges;
} pairing_t;

static void pairing_init(pairing_t *p, const char *text, size_t n) {
    memset(p, 0, sizeof *p);
    edges_init(&p->edges, text, n);
    p->owns_edges = 1;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 2; j++) p->bottoms[i][j] = -1;
}

static void pairing_free(pairing_t *p) {
    for (int i = 0; i < p->n; i++) dtok_free(&p->t[i]);
    free(p->t); free(p->stack); free(p->near2);
    if (p->owns_edges) edges_free(&p->edges);
    memset(p, 0, sizeof *p);
}

/* The probe journal's rollback, done by copy: the real state is never touched. */
static void pairing_clone(pairing_t *dst, const pairing_t *src) {
    memset(dst, 0, sizeof *dst);
    dst->n = dst->cap = src->n;
    dst->t = xrealloc(NULL, sizeof(dtok_t) * (size_t)(src->n ? src->n : 1));
    for (int i = 0; i < src->n; i++) {
        dst->t[i] = src->t[i];
        dst->t[i].opens = NULL;
        dst->t[i].nopens = dst->t[i].copens = 0;
        dst->t[i].closes = NULL;
        dst->t[i].ncloses = dst->t[i].ccloses = 0;
    }
    dst->ns = dst->cs = src->ns;
    dst->stack = xrealloc(NULL, sizeof(int) * (size_t)(src->ns + 8));
    if (src->ns) memcpy(dst->stack, src->stack, sizeof(int) * (size_t)src->ns);
    dst->cs = src->ns + 8;
    dst->c2 = src->c2;
    dst->c2cap = src->c2 + 8;
    dst->near2 = xrealloc(NULL, sizeof(int) * (size_t)dst->c2cap);
    if (src->c2) memcpy(dst->near2, src->near2, sizeof(int) * (size_t)src->c2);
    memcpy(dst->bottoms, src->bottoms, sizeof dst->bottoms);
    dst->edges = src->edges;
    dst->owns_edges = 0;
}

static void pairing_discard(pairing_t *p) {
    for (int i = 0; i < p->n; i++) { free(p->t[i].opens); free(p->t[i].closes); }
    free(p->t); free(p->stack); free(p->near2);
    memset(p, 0, sizeof *p);
}

/* _rule_of_three_refuses */
static int rule_of_three_refuses(const dtok_t *opener, const dtok_t *closer) {
    int o_len = opener->end - opener->start;
    int c_len = closer->end - closer->start;
    return (opener->can_close || closer->can_open) && ((o_len + c_len) % 3 == 0) &&
           (o_len % 3 != 0 || c_len % 3 != 0);
}

/* _span_allows */
static int span_allows(const pairing_t *p, const dtok_t *opener, const dtok_t *closer) {
    if (opener->remaining >= 2 && closer->remaining >= 2) {
        return !(edges_has_blank_line(&p->edges, opener->end, closer->start) ||
                 edges_crosses_quote_edge(&p->edges, opener->end, closer->start));
    }
    return !edges_has_newline(&p->edges, opener->end, closer->start);
}

/* _pick_opener */
static int pick_opener(pairing_t *p, const dtok_t *closer, int lowest) {
    int key0 = (closer->end - closer->start) % 3;
    int key1 = closer->can_open ? 1 : 0;
    int floor_ = p->bottoms[key0][key1];
    int nearest = -1;
    int complete = (lowest == 0);
    int cand[2];
    int ncand = 0;
    int i;
    for (int depth = p->ns - 1; depth >= lowest; depth--) {
        if (p->stack[depth] <= floor_) { complete = 1; break; }
        if (!rule_of_three_refuses(&p->t[p->stack[depth]], closer)) { nearest = depth; break; }
    }
    if (nearest < 0) {
        if (complete && p->ns > 0) p->bottoms[key0][key1] = p->stack[p->ns - 1];
        return -1;
    }
    cand[ncand++] = nearest;
    if (closer->end - closer->start == 2 && closer->remaining == 2 &&
        p->t[p->stack[nearest]].remaining < 2 && p->near2[nearest] >= 0) {
        if (p->near2[nearest] < lowest) return -1;
        if (!rule_of_three_refuses(&p->t[p->stack[p->near2[nearest]]], closer)) {
            cand[0] = p->near2[nearest];
            cand[1] = nearest;
            ncand = 2;
        }
    }
    for (i = 0; i < ncand; i++) {
        if (span_allows(p, &p->t[p->stack[cand[i]]], closer)) return cand[i];
    }
    return -1;
}

static void pairing_cut(pairing_t *p, int keep) {
    if (keep < 0) keep = 0;
    if (keep < p->ns) p->ns = keep;
    if (keep < p->c2) p->c2 = keep;
}

/* _close: pair *tok* with open runs.  idx >= 0 feeds for real (appends the
 * pairings to the tokens); idx < 0 is a probe over a snapshot. */
static void close_run(pairing_t *p, dtok_t *tok, int idx, int lowest, plist_t *out) {
    while (tok->can_close && tok->remaining > 0 && p->ns > lowest) {
        int depth = pick_opener(p, tok, lowest);
        int oi, use, kind;
        dtok_t *opener;
        if (depth < 0) break;
        pairing_cut(p, depth + 1);
        oi = p->stack[p->ns - 1];
        opener = &p->t[oi];
        use = (opener->remaining >= 2 && tok->remaining >= 2) ? 2 : 1;
        kind = (use == 2) ? K_BOLD : K_ITALIC;
        if (out) { cl_t c; c.kind = kind; c.at = oi; PUSH(out->v, out->n, out->cap, c); }
        if (idx >= 0) {
            pair_t a, b;
            a.kind = kind; a.partner = idx;
            PUSH(opener->opens, opener->nopens, opener->copens, a);
            b.kind = kind; b.partner = oi;
            PUSH(tok->closes, tok->ncloses, tok->ccloses, b);
        }
        opener->remaining -= use;
        tok->remaining -= use;
        if (opener->remaining == 0) {
            pairing_cut(p, p->ns - 1);
        } else if (opener->remaining < 2) {
            p->near2[p->c2 - 1] = (p->c2 > 1) ? p->near2[p->c2 - 2] : -1;
        }
    }
}

/* _star_token: the delimiter token for the ``*`` run, or 0 for one escaped star. */
static int star_token(const char *t, size_t n, size_t start, size_t end, dtok_t *out) {
    size_t slash = start;
    while (slash > 0 && t[slash - 1] == '\\') slash--;
    if ((start - slash) % 2) {
        start += 1;
        if (start == end) return 0;
    }
    memset(out, 0, sizeof *out);
    out->start = (int)start;
    out->end = (int)end;
    out->can_open = (end < n) && !u_isspace_at(t, n, end);
    out->can_close = (start > 0) && !u_isspace_before(t, start);
    out->remaining = (int)(end - start);
    return 1;
}

/* _StarPairing.feed */
static void pairing_feed(pairing_t *p, int idx) {
    dtok_t *tok = &p->t[idx];
    plist_t pairs;
    memset(&pairs, 0, sizeof pairs);
    close_run(p, tok, idx, 0, &pairs);
    if (tok->remaining > 0 && tok->can_open) {
        int v;
        PUSH(p->stack, p->ns, p->cs, idx);
        v = (tok->remaining >= 2) ? p->ns - 1 : (p->c2 ? p->near2[p->c2 - 1] : -1);
        PUSH(p->near2, p->c2, p->c2cap, v);
    }
    plist_free(&pairs);
}

/* ------------------------------------------------------ emphasis rules --- */

/* _pair_emphasis: the tokens of the text, each ``*`` run carrying its pairings. */
typedef struct { int is_text; str_t text; int tok; } item_t;

static void pair_emphasis(const char *t, size_t n, item_t **items_out, int *nitems_out,
                          pairing_t *pr) {
    item_t *items = NULL;
    int ni = 0, ci = 0;
    dtok_t *toks = NULL;
    int nt = 0, ct = 0;
    size_t pos = 0, i = 0;

    while (i < n) {
        size_t s, e;
        dtok_t tok;
        if (t[i] != '*') { i++; continue; }
        s = i;
        while (i < n && t[i] == '*') i++;
        e = i;
        if (!star_token(t, n, s, e, &tok)) continue;
        if ((size_t)tok.start > pos) {
            item_t it;
            it.is_text = 1;
            it.text = str_copy(t + pos, (size_t)tok.start - pos);
            it.tok = -1;
            PUSH(items, ni, ci, it);
        }
        {
            item_t it;
            it.is_text = 0;
            it.text.p = NULL;
            it.text.n = 0;
            it.tok = nt;
            PUSH(items, ni, ci, it);
        }
        PUSH(toks, nt, ct, tok);
        pos = e;
    }
    if (pos < n) {
        item_t it;
        it.is_text = 1;
        it.text = str_copy(t + pos, n - pos);
        it.tok = -1;
        PUSH(items, ni, ci, it);
    }
    pairing_init(pr, t, n);
    pr->t = toks;
    pr->n = pr->cap = nt;
    for (int k = 0; k < nt; k++) pairing_feed(pr, k);
    *items_out = items;
    *nitems_out = ni;
}

/* _render_emphasis */
static str_t render_emphasis(phstore_t *ph, item_t *items, int ni, pairing_t *pr) {
    int depth[2] = {0, 0};
    sb_t parts, group;
    int gopen = 0;

    sb_init(&parts);
    sb_init(&group);
    for (int k = 0; k < ni; k++) {
        if (items[k].is_text) {
            if (!gopen) {
                sb_addstr(&parts, items[k].text);
            } else {
                str_t esc = escape_mdv2_s(items[k].text);
                sb_addstr(&group, esc);
                free(esc.p);
            }
            continue;
        }
        {
            dtok_t *tok = &pr->t[items[k].tok];
            int ncl = tok->ncloses, nop = tok->nopens;
            int *cl = ncl ? xrealloc(NULL, sizeof(int) * (size_t)ncl) : NULL;
            int *op = nop ? xrealloc(NULL, sizeof(int) * (size_t)nop) : NULL;
            int i2;

            for (i2 = 0; i2 < ncl; i2++) cl[i2] = tok->closes[i2].kind;
            for (i2 = 0; i2 < nop; i2++) op[i2] = tok->opens[nop - 1 - i2].kind;

            /* _emphasis_marks: a style that is last to close and first to
             * reopen at one run continues unbroken. */
            for (int kind = 0; kind < 2; kind++) {
                if (ncl >= 1 && nop >= 1 && cl[ncl - 1] == kind && op[0] == kind) {
                    int cnt = 0, j;
                    for (i2 = 0; i2 < ncl; i2++)
                        if (cl[i2] == kind) cnt++;
                    if (cnt == depth[kind]) {
                        for (j = 0; j < ncl && cl[j] != kind; j++) {}
                        for (int x = j; x + 1 < ncl; x++) cl[x] = cl[x + 1];
                        ncl--;
                        for (j = 0; j < nop && op[j] != kind; j++) {}
                        for (int x = j; x + 1 < nop; x++) op[x] = op[x + 1];
                        nop--;
                    }
                }
            }
            for (i2 = 0; i2 < ncl; i2++) {
                int kind = cl[i2];
                depth[kind]--;
                if (depth[kind] == 0) sb_addc(&group, kind == K_BOLD ? '*' : '_');
            }
            if (gopen && !depth[0] && !depth[1]) {
                str_t g = sb_take(&group);
                str_t key = ph_stash(ph, g);
                sb_addstr(&parts, key);
                free(key.p);
                gopen = 0;
            }
            for (i2 = 0; i2 < tok->remaining; i2++) {
                if (!gopen) {
                    sb_addc(&parts, '*');
                } else {
                    sb_addc(&group, '\\');
                    sb_addc(&group, '*');
                }
            }
            for (i2 = 0; i2 < nop; i2++) {
                int kind = op[i2];
                if (!gopen) gopen = 1;
                if (depth[kind] == 0) sb_addc(&group, kind == K_BOLD ? '*' : '_');
                depth[kind]++;
            }
            free(cl);
            free(op);
        }
    }
    free(group.p);
    return sb_take(&parts);
}

/* _resolve_emphasis */
static str_t resolve_emphasis(str_t text, phstore_t *ph) {
    item_t *items = NULL;
    int ni = 0;
    pairing_t pr;
    str_t out;

    pair_emphasis(text.p, text.n, &items, &ni, &pr);
    out = render_emphasis(ph, items, ni, &pr);
    for (int k = 0; k < ni; k++) {
        if (items[k].is_text) free(items[k].text.p);
    }
    free(items);
    pairing_free(&pr);
    return out;
}

/* ------------------------------------------------------------- lines ----- */

typedef struct { size_t s, e; } line_t;

static line_t *lines_of(str_t t, int *count) {
    line_t *v = NULL;
    int n = 0, c = 0;
    size_t start = 0, i;
    for (i = 0; i <= t.n; i++) {
        if (i == t.n || t.p[i] == '\n') {
            line_t ln;
            ln.s = start;
            ln.e = i;
            PUSH(v, n, c, ln);
            start = i + 1;
        }
    }
    *count = n;
    return v;
}

/* ------------------------------------------------ step 1: fenced code ---- */

static str_t protect_fenced(str_t text, phstore_t *ph) {
    int nl = 0;
    line_t *L = lines_of(text, &nl);
    sb_t out;
    size_t pos = 0;
    int i = 0;

    sb_init(&out);
    while (i < nl) {
        size_t ls = L[i].s, le = L[i].e;
        long idx = find_bytes(text.p, text.n, ls, "```", 3);
        int open_ok = 0;

        if (idx >= 0 && (size_t)idx + 3 <= le && le < text.n) {
            size_t k;
            int bad = 0;
            for (k = (size_t)idx + 3; k < le; k++) {
                if (text.p[k] == '`') { bad = 1; break; }
            }
            if (!bad) open_ok = 1;
        }
        if (open_ok) {
            int j;
            for (j = i + 1; j < nl; j++) {
                size_t p = L[j].s, q = L[j].e;
                while (p < q && (text.p[p] == ' ' || text.p[p] == '\t')) p++;
                if (q - p >= 3 && memcmp(text.p + p, "```", 3) == 0) {
                    size_t r = p + 3;
                    while (r < q && (text.p[r] == ' ' || text.p[r] == '\t')) r++;
                    if (r == q || (r + 1 == q && text.p[r] == '\r')) break;
                }
            }
            if (j < nl) {
                size_t on = (size_t)idx;
                size_t oend = le + 1;
                size_t cl_start = L[j].s, cl_end;
                sb_t phv;
                str_t v, key;
                size_t k;
                size_t p;

                for (p = cl_start; p < L[j].e && (text.p[p] == ' ' || text.p[p] == '\t'); p++) {}
                cl_end = p + 3;
                sb_init(&phv);
                sb_addn(&out, text.p + pos, on - pos);
                sb_addn(&phv, text.p + on, oend - on);
                for (k = oend; k < cl_start; k++) {
                    if (text.p[k] == '\\') { sb_addc(&phv, '\\'); sb_addc(&phv, '\\'); }
                    else if (text.p[k] == '`') { sb_addc(&phv, '\\'); sb_addc(&phv, '`'); }
                    else sb_addc(&phv, text.p[k]);
                }
                sb_addn(&phv, text.p + cl_start, cl_end - cl_start);
                v = sb_take(&phv);
                key = ph_stash(ph, v);
                sb_addstr(&out, key);
                free(key.p);
                pos = L[j].e;
                i = j + 1;
                continue;
            }
        }
        i++;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    free(L);
    return sb_take(&out);
}

/* ------------------------------------------------ step 2: inline code ---- */

typedef struct { size_t s, e; } run_t;

static str_t stash_inline_code(str_t text, phstore_t *ph) {
    run_t *runs = NULL;
    int nr = 0, cr = 0;
    int *breaks = NULL;
    int nb = 0, cb = 0;
    int *next_same = NULL;
    int *last_by_width = NULL;
    char *literal = NULL;
    sb_t out;
    size_t pos = 0, i;
    int k;

    for (i = 0; i < text.n; i++) {
        if (text.p[i] == '`') {
            size_t s = i;
            while (i < text.n && text.p[i] == '`') i++;
            { run_t r; r.s = s; r.e = i; PUSH(runs, nr, cr, r); }
        }
        if (text.p[i] == '\r' || text.p[i] == '\n') PUSH(breaks, nb, cb, (int)i);
    }
    PUSH(breaks, nb, cb, (int)text.n);
    next_same = xrealloc(NULL, sizeof(int) * (size_t)(nr ? nr : 1));
    last_by_width = xrealloc(NULL, sizeof(int) * (text.n + 2));
    literal = xrealloc(NULL, (size_t)(nr ? nr : 1));
    for (k = 0; k < (int)text.n + 2; k++) last_by_width[k] = -1;
    for (k = 0; k < nr; k++) literal[k] = 0;
    for (k = nr - 1; k >= 0; k--) {
        int width = (int)(runs[k].e - runs[k].s);
        next_same[k] = last_by_width[width];
        last_by_width[width] = k;
    }
    (void)cb;

    sb_init(&out);
    i = 0;
    k = 0;
    while (k < nr) {
        run_t opener = runs[k];
        int width = (int)(opener.e - opener.s);
        int close = next_same[k];

        if (close >= 0) {
            int bi = bisect_left(breaks, nb, (int)opener.e);
            int bound = breaks[bi];
            if ((int)runs[close].s > bound) close = -1;
        }
        if (literal[k] || close < 0) { k++; continue; }
        {
            size_t cs = runs[close].s;
            str_t content = str_slice(text, opener.e, cs);
            int has_tick = memchr(content.p, '`', content.n) != NULL;
            if (width > 2 || (width == 2 && has_tick)) {
                literal[close] = 1;
                free(content.p);
                k++;
                continue;
            }
            {
                sb_t phv;
                str_t v, key;
                size_t x;
                sb_init(&phv);
                sb_addc(&phv, '`');
                for (x = 0; x < content.n; x++) {
                    if (content.p[x] == '\\') { sb_addc(&phv, '\\'); sb_addc(&phv, '\\'); }
                    else if (content.p[x] == '`') { sb_addc(&phv, '\\'); sb_addc(&phv, '`'); }
                    else sb_addc(&phv, content.p[x]);
                }
                sb_addc(&phv, '`');
                sb_addn(&out, text.p + pos, opener.s - pos);
                v = sb_take(&phv);
                key = ph_stash(ph, v);
                sb_addstr(&out, key);
                free(key.p);
                free(content.p);
                pos = runs[close].e;
                k = close + 1;
            }
        }
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    free(runs);
    free(breaks);
    free(next_same);
    free(last_by_width);
    free(literal);
    return sb_take(&out);
}

/* --------------------------------------------------- step 3: md links ---- */

typedef struct {
    size_t start, end, label_s, label_e, target_s, target_e;
} link_t;

static int link_at(str_t t, size_t i, link_t *m) {
    size_t j, p;
    if (t.p[i] != '[') return 0;
    j = i + 1;
    while (j < t.n && t.p[j] != ']') j++;
    if (j >= t.n || j == i + 1) return 0;
    if (j + 1 >= t.n || t.p[j + 1] != '(') return 0;
    m->start = i;
    m->label_s = i + 1;
    m->label_e = j;
    p = j + 2;
    m->target_s = p;
    for (;;) {
        while (p < t.n && t.p[p] != '(' && t.p[p] != ')') p++;
        if (p >= t.n) return 0;
        if (t.p[p] == '(') {
            size_t q = p + 1;
            while (q < t.n && t.p[q] != '(' && t.p[q] != ')') q++;
            if (q >= t.n || t.p[q] != ')') return 0;
            p = q + 1;
            continue;
        }
        m->target_e = p;
        m->end = p + 1;
        return 1;
    }
}

static int str_target_renders(str_t s) {
    size_t a = 0, b = s.n;
    while (a < b && u_isspace_at(s.p, s.n, a)) a++;
    while (b > a && u_isspace_before(s.p, b)) b--;
    return target_renders_raw(s.p ? s.p + a : "", b - a);
}

static str_t convert_links(str_t text, phstore_t *ph) {
    sb_t out;
    size_t pos = 0, i = 0;

    sb_init(&out);
    while (i < text.n) {
        link_t m;
        if (text.p[i] == '[' && link_at(text, i, &m)) {
            sb_addn(&out, text.p + pos, m.start - pos);
            if (crosses_quote_edge_raw(text.p, text.n, m.start, m.end)) {
                sb_addn(&out, text.p + m.start, m.end - m.start);
            } else {
                str_t target = str_slice(text, m.target_s, m.target_e);
                str_t label = str_slice(text, m.label_s, m.label_e);
                str_t elabel = escape_mdv2_s(label);
                if (!str_target_renders(target)) {
                    str_t key = ph_stash(ph, elabel);
                    sb_addstr(&out, key);
                    free(key.p);
                } else {
                    sb_t phv;
                    str_t v, key;
                    size_t x;
                    sb_init(&phv);
                    sb_addc(&phv, '[');
                    sb_addstr(&phv, elabel);
                    sb_adds(&phv, "](");
                    for (x = 0; x < target.n; x++) {
                        if (target.p[x] == '\\') { sb_addc(&phv, '\\'); sb_addc(&phv, '\\'); }
                        else if (target.p[x] == ')') { sb_addc(&phv, '\\'); sb_addc(&phv, ')'); }
                        else sb_addc(&phv, target.p[x]);
                    }
                    sb_addc(&phv, ')');
                    v = sb_take(&phv);
                    key = ph_stash(ph, v);
                    sb_addstr(&out, key);
                    free(key.p);
                    free(elabel.p);
                }
                free(target.p);
                free(label.p);
            }
            pos = m.end;
            i = m.end;
            continue;
        }
        i++;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    return sb_take(&out);
}

/* ------------------------------------------------------ step 4: headers --- */

static str_t convert_headers(str_t text, phstore_t *ph) {
    sb_t out;
    size_t pos = 0, i = 0;

    sb_init(&out);
    while (i <= text.n) {
        size_t p = i, q;
        int hashes = 0;
        /* at a line start */
        while (p < text.n && text.p[p] == '#' && hashes < 7) { p++; hashes++; }
        if (hashes >= 1 && hashes <= 6 && p < text.n && u_isspace_at(text.p, text.n, p)) {
            while (p < text.n && u_isspace_at(text.p, text.n, p)) p++;
            if (p < text.n) {
                q = p;
                while (q < text.n && text.p[q] != '\n') q++;
                if (q > p) {
                    str_t inner = str_slice(text, p, q);
                    str_t stripped, dedup, esc, key;
                    sb_t phv;
                    size_t a = 0, b = inner.n;
                    /* strip */
                    while (a < b && u_isspace_at(inner.p, inner.n, a)) a++;
                    while (b > a && u_isspace_before(inner.p, b)) b--;
                    stripped = str_slice(inner, a, b);
                    /* re.sub(r'\*\*(.+?)\*\*', r'\1', inner) */
                    {
                        sb_t d;
                        size_t x = 0, last = 0;
                        sb_init(&d);
                        while (x < stripped.n) {
                            if (stripped.p[x] == '*' && x + 1 < stripped.n && stripped.p[x + 1] == '*' &&
                                x + 2 < stripped.n && stripped.p[x + 2] != '\n') {
                                size_t y = x + 3;
                                int found = 0;
                                while (y < stripped.n && stripped.p[y] != '\n') {
                                    if (stripped.p[y] == '*' && y + 1 < stripped.n &&
                                        stripped.p[y + 1] == '*') {
                                        found = 1;
                                        break;
                                    }
                                    y++;
                                }
                                if (found) {
                                    sb_addn(&d, stripped.p + last, x - last);
                                    sb_addn(&d, stripped.p + x + 2, y - (x + 2));
                                    last = y + 2;
                                    x = last;
                                    continue;
                                }
                            }
                            x++;
                        }
                        sb_addn(&d, stripped.p + last, stripped.n - last);
                        dedup = sb_take(&d);
                    }
                    esc = escape_mdv2_s(dedup);
                    sb_init(&phv);
                    sb_addc(&phv, '*');
                    sb_addstr(&phv, esc);
                    sb_addc(&phv, '*');
                    sb_addn(&out, text.p + pos, i - pos);
                    {
                        str_t v = sb_take(&phv);
                        key = ph_stash(ph, v);
                        sb_addstr(&out, key);
                        free(key.p);
                    }
                    pos = q;
                    i = q;
                    free(inner.p);
                    free(stripped.p);
                    free(dedup.p);
                    free(esc.p);
                    continue;
                }
            }
        }
        /* next line */
        while (i < text.n && text.p[i] != '\n') i++;
        if (i >= text.n) break;
        i++;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    return sb_take(&out);
}

/* -------------------------------- step 4b: expandable-quote opener ------- */

static str_t hide_expandable_quote(str_t text) {
    sb_t out;
    size_t pos = 0, i = 0;

    sb_init(&out);
    while (i < text.n) {
        if (text.p[i] == '*' && i + 1 < text.n && text.p[i + 1] == '*') {
            size_t p = i + 2;
            int gt = 0;
            while (p < text.n && text.p[p] == '>' && gt < 3) { p++; gt++; }
            if (gt >= 1 && p < text.n && text.p[p] == ' ') {
                sb_addn(&out, text.p + pos, i - pos);
                sb_addn(&out, EQ_SENTINEL, EQ_LEN);
                pos = i + 2;
                i = i + 2;
                continue;
            }
        }
        while (i < text.n && text.p[i] != '\n') i++;
        if (i >= text.n) break;
        i++;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    return sb_take(&out);
}

/* ------------------------------------- step 6/7: strike and spoiler ------- */

static str_t stash_delimited(str_t text, const char *delim, phstore_t *ph, const char *wrap) {
    size_t dl = strlen(delim);
    sb_t out;
    size_t pos = 0, i = 0;

    sb_init(&out);
    while (i + dl <= text.n) {
        if (memcmp(text.p + i, delim, dl) == 0) {
            size_t j = i + dl;
            int found = 0;
            while (j + dl <= text.n && text.p[j] != '\n') {
                if (j > i + dl && memcmp(text.p + j, delim, dl) == 0) { found = 1; break; }
                j++;
            }
            if (found) {
                str_t content = str_slice(text, i + dl, j);
                str_t esc = escape_mdv2_s(content);
                sb_t phv;
                str_t v, key;
                sb_addn(&out, text.p + pos, i - pos);
                sb_init(&phv);
                sb_adds(&phv, wrap);
                sb_addstr(&phv, esc);
                sb_adds(&phv, wrap);
                v = sb_take(&phv);
                key = ph_stash(ph, v);
                sb_addstr(&out, key);
                free(key.p);
                free(content.p);
                free(esc.p);
                pos = j + dl;
                i = pos;
                continue;
            }
        }
        i++;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    return sb_take(&out);
}

/* ------------------------------------------------- step 9: blockquotes ---- */

static int quote_prefix_len(str_t t, size_t p, size_t e, int allow_sentinel, size_t *plen) {
    size_t i = p;
    int gt = 0;
    if (allow_sentinel && e - i >= EQ_LEN && memcmp(t.p + i, EQ_SENTINEL, EQ_LEN) == 0) i += EQ_LEN;
    while (i < e && t.p[i] == '>' && gt < 3) { i++; gt++; }
    if (gt < 1) return 0;
    if (i >= e || t.p[i] != ' ') return 0;
    if (i + 1 >= e) return 0;
    *plen = i - p;
    return 1;
}

static str_t convert_blockquotes(str_t text, phstore_t *ph) {
    int nl = 0;
    line_t *L = lines_of(text, &nl);
    sb_t out;
    size_t pos = 0;
    int i = 0;

    sb_init(&out);
    while (i < nl) {
        size_t plen;
        if (quote_prefix_len(text, L[i].s, L[i].e, 1, &plen)) {
            int j = i + 1;
            size_t plen2;
            int expandable;
            int last;
            while (j < nl && quote_prefix_len(text, L[j].s, L[j].e, 0, &plen2)) j++;
            sb_addn(&out, text.p + pos, L[i].s - pos);
            expandable = (L[i].e - L[i].s >= EQ_LEN &&
                          memcmp(text.p + L[i].s, EQ_SENTINEL, EQ_LEN) == 0);
            last = -1;
            if (expandable) {
                size_t cs, ce;
                size_t p2 = L[j - 1].s;
                size_t pl2;
                (void)quote_prefix_len(text, L[j - 1].s, L[j - 1].e, 0, &pl2);
                cs = p2 + pl2 + 1;
                ce = L[j - 1].e;
                if (ce - cs >= 2 && memcmp(text.p + ce - 2, "||", 2) == 0) last = j - 1;
            }
            for (int k = i; k < j; k++) {
                size_t pl;
                sb_t phv;
                str_t v, key;
                size_t ps, ce;
                (void)quote_prefix_len(text, L[k].s, L[k].e, k == i, &pl);
                ps = L[k].s;
                ce = L[k].e;
                sb_init(&phv);
                /* prefix, with the sentinel restored to a literal ** */
                if (k == i && expandable) {
                    sb_adds(&phv, "**");
                    sb_addn(&phv, text.p + ps + EQ_LEN, pl - EQ_LEN);
                } else {
                    sb_addn(&phv, text.p + ps, pl);
                }
                sb_addc(&phv, ' ');
                {
                    size_t cs = ps + pl + 1;
                    size_t cend = ce;
                    if (k == last) cend -= 2;
                    {
                        str_t content = str_slice(text, cs, cend);
                        str_t esc = escape_mdv2_s(content);
                        sb_addstr(&phv, esc);
                        free(content.p);
                        free(esc.p);
                    }
                }
                if (k == last) sb_adds(&phv, "||");
                v = sb_take(&phv);
                key = ph_stash(ph, v);
                sb_addstr(&out, key);
                free(key.p);
                if (k + 1 < j) sb_addc(&out, '\n');
            }
            pos = L[j - 1].e;
            i = j;
            continue;
        }
        i++;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    free(L);
    return sb_take(&out);
}

/* ------------------------------------- step 12: the bracket safety net ---- */

/* _escape_bare_bracket */
static str_t escape_bare_brackets_in_seg(str_t seg) {
    sb_t out;
    size_t s;
    sb_init(&out);
    for (s = 0; s < seg.n; s++) {
        char ch = seg.p[s];
        if (ch == '(' || ch == ')' || ch == '{' || ch == '}') {
            int keep = 0;
            if (s > 0 && seg.p[s - 1] == '\\') {
                keep = 1;
            } else if (ch == '(' && s > 0 && seg.p[s - 1] == ']') {
                keep = 1;
            } else if (ch == ')') {
                if (find_bytes(seg.p, s, 0, "](", 2) >= 0) {
                    int depth = 0;
                    long lo = (long)s - 2000;
                    if (lo < -1) lo = -1;
                    for (long j = (long)s - 1; j > lo; j--) {
                        if (seg.p[j] == '(') {
                            depth--;
                            if (depth < 0) {
                                if (j > 0 && seg.p[j - 1] == ']') keep = 1;
                                break;
                            }
                        } else if (seg.p[j] == ')') {
                            depth++;
                        }
                    }
                }
            }
            if (!keep) sb_addc(&out, '\\');
        }
        sb_addc(&out, ch);
    }
    return sb_take(&out);
}

static void append_prose(sb_t *out, str_t seg) {
    str_t e = escape_bare_brackets_in_seg(seg);
    sb_addstr(out, e);
    free(e.p);
}

static str_t safety_net(str_t text) {
    sb_t out;
    size_t pos = 0, i = 0;

    sb_init(&out);
    while (i < text.n) {
        if (text.p[i] == '`') {
            int triple = (i + 2 < text.n && text.p[i + 1] == '`' && text.p[i + 2] == '`');
            size_t code_end = 0;
            int code = 0;
            if (triple) {
                long j = find_bytes(text.p, text.n, i + 3, "```", 3);
                if (j >= 0) { code = 1; code_end = (size_t)j + 3; }
            } else {
                size_t j = i + 1;
                while (j < text.n && text.p[j] != '`' && text.p[j] != '\n' && text.p[j] != '\r') j++;
                if (j < text.n && text.p[j] == '`' && j > i + 1) { code = 1; code_end = j + 1; }
            }
            if (code) {
                str_t seg = str_slice(text, pos, i);
                append_prose(&out, seg);
                free(seg.p);
                sb_addn(&out, text.p + i, code_end - i);
                pos = code_end;
                i = code_end;
                continue;
            }
        }
        i++;
    }
    {
        str_t seg = str_slice(text, pos, text.n);
        append_prose(&out, seg);
        free(seg.p);
    }
    return sb_take(&out);
}

/* --------------------------------------- step 3b: bare URLs + emphasis ---- */

/* _URL_PROSE_AFTER = ".,;:!?\'")]}" */
static int prose_follows(const char *u, size_t n, size_t end) {
    unsigned char c;
    if (end >= n) return 1;
    c = (unsigned char)u[end];
    if (c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' || c == '\'' ||
        c == '"' || c == ')' || c == ']' || c == '}')
        return 1;
    return c >= 0x80; /* not nxt.isascii() */
}

/* _escaped */
static int escaped_at(const char *t, size_t pos) {
    size_t slash = pos;
    while (slash > 0 && t[slash - 1] == '\\') slash--;
    return (pos - slash) % 2 == 1;
}

/* _Opener, and the fallback closer list it shares with its siblings */
typedef struct { int flag; size_t start, stop; } fb_t;

typedef struct opener {
    int alive, spent;
    int is_star;
    int tok;
    int markkind;
    fb_t *close;
} opener_t;

enum { BM_STRIKE = 0, BM_SPOILER = 1, BM_DSTAR = 2, BM_SSTAR = 3 };

typedef struct { opener_t **v; int n, cap; } olist_t;

typedef struct { size_t key, val; } endpair_t;

typedef struct {
    str_t text;
    opener_t **stack; int ns, cs;
    olist_t by_mark[4];
    pairing_t stars;
    opener_t **star_openers; int nso, cso;
    endpair_t *ends; int nends, cends;
    fb_t **fbs; int nfb, cfb;
    opener_t **all; int nall, call;   /* every opener allocated, freed once */
} uscan_t;

static opener_t *new_opener(uscan_t *s, int is_star, int tok, int markkind) {
    opener_t *o = xrealloc(NULL, sizeof *o);
    memset(o, 0, sizeof *o);
    o->alive = 1;
    o->is_star = is_star;
    o->tok = tok;
    o->markkind = markkind;
    PUSH(s->all, s->nall, s->call, o);
    return o;
}

static void scan_push(uscan_t *s, opener_t *o) { PUSH(s->stack, s->ns, s->cs, o); }

static void ends_setdefault(uscan_t *s, size_t key, size_t val) {
    for (int i = 0; i < s->nends; i++) {
        if (s->ends[i].key == key) return;
    }
    { endpair_t e; e.key = key; e.val = val; PUSH(s->ends, s->nends, s->cends, e); }
}

static int ends_get(uscan_t *s, size_t key, size_t *val) {
    for (int i = 0; i < s->nends; i++) {
        if (s->ends[i].key == key) { *val = s->ends[i].val; return 1; }
    }
    return 0;
}

/* _release */
static void scan_release(uscan_t *s, opener_t **arr, int n, int is_stack) {
    int i;
    for (i = 0; i < n; i++) {
        opener_t *o = arr[i];
        if (o->alive && o->close && o->close->flag) ends_setdefault(s, o->close->start, o->close->stop);
        o->alive = 0;
    }
    if (is_stack) {
        s->ns = 0;
        for (i = 0; i < 4; i++) s->by_mark[i].n = 0;
    }
}

/* _single_line_openers: strike, spoiler, and ``*`` runs with other than two
 * stars left.  A run filed under ``*`` that is down to two stars survives
 * (it is filed under ``**`` too). */
static void single_line_openers(uscan_t *s, opener_t ***out, int *nout) {
    opener_t **v = NULL;
    int n = 0, c = 0;
    int i;
    for (i = 0; i < s->by_mark[BM_STRIKE].n; i++) PUSH(v, n, c, s->by_mark[BM_STRIKE].v[i]);
    s->by_mark[BM_STRIKE].n = 0;
    for (i = 0; i < s->by_mark[BM_SPOILER].n; i++) PUSH(v, n, c, s->by_mark[BM_SPOILER].v[i]);
    s->by_mark[BM_SPOILER].n = 0;
    for (i = 0; i < s->by_mark[BM_SSTAR].n; i++) {
        opener_t *o = s->by_mark[BM_SSTAR].v[i];
        if (s->stars.t[o->tok].remaining != 2) PUSH(v, n, c, o);
    }
    s->by_mark[BM_SSTAR].n = 0;
    *out = v;
    *nout = n;
}

/* _unwind */
static void scan_unwind(uscan_t *s, opener_t *target) {
    while (s->ns > 0 && s->stack[s->ns - 1] != target) {
        opener_t *top = s->stack[s->ns - 1];
        s->ns--;
        scan_release(s, &top, 1, 0);
    }
}

/* _file_star */
static void scan_file_star(uscan_t *s, opener_t *o) {
    int bucket = (s->stars.t[o->tok].remaining == 2) ? BM_DSTAR : BM_SSTAR;
    olist_t *b = &s->by_mark[bucket];
    PUSH(b->v, b->n, b->cap, o);
}

/* _delimiter */
static void scan_delimiter(uscan_t *s, int markkind) {
    olist_t *same = &s->by_mark[markkind];
    while (same->n && !same->v[same->n - 1]->alive) same->n--;
    if (same->n) {
        opener_t *target = same->v[--same->n];
        scan_unwind(s, target);
        s->ns--;
        target->alive = 0;
        if (target->close) target->close->flag = 0;
    } else {
        opener_t *o = new_opener(s, 0, -1, markkind);
        scan_push(s, o);
        PUSH(same->v, same->n, same->cap, o);
    }
}

/* _StarPairing.feed, with the pairs it made handed back. */
static void feed_with_pairs(pairing_t *p, int idx, plist_t *out) {
    dtok_t *tok = &p->t[idx];
    close_run(p, tok, idx, 0, out);
    if (tok->remaining > 0 && tok->can_open) {
        int v;
        PUSH(p->stack, p->ns, p->cs, idx);
        v = (tok->remaining >= 2) ? p->ns - 1 : (p->c2 ? p->near2[p->c2 - 1] : -1);
        PUSH(p->near2, p->c2, p->c2cap, v);
    }
}

static void scan_star_run(uscan_t *s, size_t start, size_t end) {
    dtok_t tok;
    int idx;
    plist_t pairs;
    if (!star_token(s->text.p, s->text.n, start, end, &tok)) return;
    idx = s->stars.n;
    PUSH(s->stars.t, s->stars.n, s->stars.cap, tok);
    while (s->nso <= idx) PUSH(s->star_openers, s->nso, s->cso, (opener_t *)NULL);
    memset(&pairs, 0, sizeof pairs);
    feed_with_pairs(&s->stars, idx, &pairs);
    for (int k = 0; k < pairs.n; k++) {
        int at = pairs.v[k].at;
        opener_t *o = s->star_openers[at];
        if (!o || !o->alive) continue;
        scan_unwind(s, o);
        if (o->close && (s->stars.t[at].remaining == 0 || !o->spent)) o->close->flag = 0;
        if (s->stars.t[at].remaining == 0) {
            o->alive = 0;
            s->ns--;
        } else {
            scan_file_star(s, o);
        }
    }
    plist_free(&pairs);
    if (s->stars.t[idx].remaining > 0 && s->stars.t[idx].can_open) {
        opener_t *o = new_opener(s, 1, idx, -1);
        s->star_openers[idx] = o;
        scan_push(s, o);
        scan_file_star(s, o);
    }
}

static int mark_matches(str_t run, size_t pos, int markkind) {
    if (markkind == BM_STRIKE) return pos + 1 < run.n && run.p[pos] == '~' && run.p[pos + 1] == '~';
    return pos + 1 < run.n && run.p[pos] == '|' && run.p[pos + 1] == '|';
}

/* _star_closes: the ``*`` openers a URL star run pairs with, and whether stars
 * are left over. */
static int scan_star_closes(uscan_t *s, pairing_t *snap, size_t start, size_t end,
                            opener_t **closed, int *nclosed, int *spare) {
    dtok_t tok;
    plist_t pairs;
    int i;
    *nclosed = 0;
    *spare = 0;
    if (!star_token(s->text.p, s->text.n, start, end, &tok)) return 0;
    memset(&pairs, 0, sizeof pairs);
    close_run(snap, &tok, -1, (s->stars.ns - 8 > 0) ? s->stars.ns - 8 : 0, &pairs);
    for (i = 0; i < pairs.n && *nclosed < 8; i++) {
        opener_t *o = s->star_openers[pairs.v[i].at];
        int seen = 0;
        for (int j = 0; j < *nclosed; j++)
            if (closed[j] == o) { seen = 1; break; }
        if (!seen) closed[(*nclosed)++] = o;
    }
    plist_free(&pairs);
    *spare = tok.remaining > 0;
    return *nclosed > 0;
}

static int live_index(opener_t **live, int nlive, int from, opener_t *o) {
    for (int k = from; k < nlive; k++)
        if (live[k] == o) return k;
    return -1;
}

/* _innermost: the live openers, innermost first, that *run* closes, or none.
 * One snapshot for the whole call: the Python journal only rolls back when
 * _innermost returns, so probes see each other's cuts and ``spent`` is read
 * from the still-consumed token remainings. */
static int scan_innermost(uscan_t *s, size_t at, str_t run, opener_t **taken, int *ntaken) {
    opener_t *live[8];
    int nlive = 0;
    size_t pos = 0;
    int reach = 0;
    int ok = 1;
    pairing_t snap;

    *ntaken = 0;
    while (s->ns > 0 && !s->stack[s->ns - 1]->alive) s->ns--;
    for (int k = s->ns - 1; k >= 0 && k >= s->ns - 8; k--) {
        if (s->stack[k]->alive) live[nlive++] = s->stack[k];
    }
    pairing_clone(&snap, &s->stars);
    while (pos < run.n) {
        opener_t *closed[8];
        int nclosed = 0, spare = 0, got;
        if (run.p[pos] == '*') {
            size_t q = pos;
            while (q < run.n && run.p[q] == '*') q++;
            got = scan_star_closes(s, &snap, at + pos, at + q, closed, &nclosed, &spare);
            pos = q;
            if (spare && pos < run.n) { ok = 0; break; }
        } else {
            opener_t *nxt = (reach < nlive) ? live[reach] : NULL;
            if (nxt && !nxt->is_star && mark_matches(run, pos, nxt->markkind)) {
                closed[0] = nxt;
                nclosed = 1;
                got = 1;
            } else {
                got = 0;
            }
            pos += 2;
        }
        if (!got) { ok = 0; break; }
        for (int i = 0; i < nclosed; i++) {
            if (closed[i]->close || live_index(live, nlive, reach, closed[i]) < 0) { ok = 0; break; }
        }
        if (!ok) break;
        if (pos < run.n) {
            for (int i = 0; i < nclosed; i++) {
                if (closed[i]->is_star && snap.t[closed[i]->tok].remaining > 0) { ok = 0; break; }
            }
        }
        if (!ok) break;
        {
            int maxi = -1;
            for (int i = 0; i < nclosed; i++) {
                int li = live_index(live, nlive, 0, closed[i]);
                if (li > maxi) maxi = li;
            }
            reach = maxi + 1;
        }
        for (int i = 0; i < nclosed; i++) {
            if (*ntaken < 8) taken[(*ntaken)++] = closed[i];
        }
    }
    if (ok) {
        for (int i = 0; i < *ntaken; i++) {
            opener_t *o = taken[i];
            o->spent = (!o->is_star) || (snap.t[o->tok].remaining == 0);
        }
    } else {
        *ntaken = 0;
    }
    pairing_discard(&snap);
    return ok && *ntaken > 0;
}

/* _url */
static void scan_url(uscan_t *s, size_t mstart, str_t url) {
    size_t p = 0;
    while (p < url.n) {
        size_t q, start;
        if (url.p[p] != '*' && url.p[p] != '~' && url.p[p] != '|') { p++; continue; }
        q = p;
        while (q < url.n && (url.p[q] == '*' || url.p[q] == '~' || url.p[q] == '|')) q++;
        if (prose_follows(url.p, url.n, q)) {
            start = p + (escaped_at(url.p, p) ? 1 : 0);
            if (start != q) {
                str_t run = str_slice(url, start, q);
                opener_t *taken[8];
                int ntaken = 0;
                if (scan_innermost(s, mstart + start, run, taken, &ntaken)) {
                    fb_t *fb = xrealloc(NULL, sizeof *fb);
                    fb->flag = 1;
                    fb->start = mstart;
                    fb->stop = mstart + url_end_len(url.p, start);
                    PUSH(s->fbs, s->nfb, s->cfb, fb);
                    for (int i = 0; i < ntaken; i++) taken[i]->close = fb;
                    free(run.p);
                    return;
                }
                free(run.p);
            }
        }
        p = q;
    }
}

/* _prose */
static void scan_prose(uscan_t *s, size_t start, size_t end) {
    size_t p = start;
    str_t t = s->text;
    while (p < end) {
        if (t.p[p] == '\n') {
            size_t q = p;
            while (q < end && t.p[q] == '\n') q++;
            if (q - p > 1) {
                scan_release(s, s->stack, s->ns, 1);
            } else {
                opener_t **arr = NULL;
                int n = 0;
                single_line_openers(s, &arr, &n);
                scan_release(s, arr, n, 0);
                free(arr);
            }
            p = q;
        } else if (t.p[p] == '*') {
            size_t q = p;
            while (q < end && t.p[q] == '*') q++;
            scan_star_run(s, p, q);
            p = q;
        } else if (t.p[p] == '~' && p + 1 < end && t.p[p + 1] == '~') {
            if (!(p && t.p[p - 1] == '\\')) scan_delimiter(s, BM_STRIKE);
            p += 2;
        } else if (t.p[p] == '|' && p + 1 < end && t.p[p + 1] == '|') {
            if (!(p && t.p[p - 1] == '\\')) scan_delimiter(s, BM_SPOILER);
            p += 2;
        } else {
            p++;
        }
    }
}

static void scan_free(uscan_t *s) {
    for (int i = 0; i < s->nall; i++) free(s->all[i]);
    free(s->all);
    for (int i = 0; i < s->nfb; i++) free(s->fbs[i]);
    free(s->fbs);
    for (int i = 0; i < 4; i++) free(s->by_mark[i].v);
    free(s->stack);
    free(s->star_openers);
    free(s->ends);
    pairing_free(&s->stars);
}

/* _stash_bare_urls */
static int url_at(str_t t, size_t i, size_t *end) {
    size_t p, q;
    if (!ascii_alpha(t.p[i])) return 0;
    if (i > 0 && u_isword_before(t.p, i)) return 0; /* no \b before the scheme */
    p = i + 1;
    while (p < t.n && (ascii_alnum(t.p[p]) || t.p[p] == '+' || t.p[p] == '.' || t.p[p] == '-')) p++;
    if (p + 3 > t.n || memcmp(t.p + p, "://", 3) != 0) return 0;
    p += 3;
    q = p;
    while (q < t.n) {
        unsigned char c = (unsigned char)t.p[q];
        size_t clen = 1;
        if (c == '<' || c == '>' || c == 0) break;
        if (u_isspace_at(t.p, t.n, q)) break;
        if (c >= 0x80) (void)u_decode((const unsigned char *)t.p + q, t.n - q, &clen);
        q += clen;
    }
    if (q == p) return 0;
    *end = q;
    return 1;
}

typedef struct { size_t s, e; } urange_t;

static str_t stash_bare_urls(str_t text, phstore_t *ph) {
    urange_t *urls = NULL;
    int nu = 0, cu = 0;
    size_t i = 0;
    uscan_t scan;
    sb_t out;
    size_t pos = 0;

    while (i < text.n) {
        size_t e;
        if (text.p[i] && url_at(text, i, &e)) {
            urange_t u;
            u.s = i;
            u.e = e;
            PUSH(urls, nu, cu, u);
            i = e;
            continue;
        }
        i++;
    }
    if (!nu) {
        free(urls);
        return text;
    }
    memset(&scan, 0, sizeof scan);
    scan.text = text;
    pairing_init(&scan.stars, text.p, text.n);
    {
        size_t p = 0;
        for (int k = 0; k < nu; k++) {
            str_t url;
            scan_prose(&scan, p, urls[k].s);
            url = str_slice(text, urls[k].s, urls[k].e);
            scan_url(&scan, urls[k].s, url);
            free(url.p);
            p = urls[k].e;
        }
        scan_prose(&scan, p, text.n);
        scan_release(&scan, scan.stack, scan.ns, 1);
    }
    sb_init(&out);
    for (int k = 0; k < nu; k++) {
        size_t stop;
        str_t chunk, esc, key;
        if (!ends_get(&scan, urls[k].s, &stop))
            stop = urls[k].s + url_end_len(text.p + urls[k].s, urls[k].e - urls[k].s);
        sb_addn(&out, text.p + pos, urls[k].s - pos);
        chunk = str_slice(text, urls[k].s, stop);
        esc = escape_mdv2_s(chunk);
        key = ph_stash(ph, esc);
        sb_addstr(&out, key);
        free(key.p);
        free(chunk.p);
        pos = stop;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    scan_free(&scan);
    free(urls);
    free(text.p);
    return sb_take(&out);
}

/* --------------------------------------------------- the public entry ----- */

static w31_telegramma_02bbc8_p1_prose_fn g_latex_hook = NULL;
static w31_telegramma_02bbc8_p1_prose_fn g_tables_hook = NULL;
static void *g_seam_ctx = NULL;

void w31_telegramma_02bbc8_p1_set_seams(w31_telegramma_02bbc8_p1_prose_fn latex,
                                        w31_telegramma_02bbc8_p1_prose_fn tables, void *ctx) {
    g_latex_hook = latex;
    g_tables_hook = tables;
    g_seam_ctx = ctx;
}

static void run_hook(w31_telegramma_02bbc8_p1_prose_fn hook, str_t *text) {
    char *r;
    if (!hook) return;
    r = hook(text->p, g_seam_ctx);
    if (r) {
        free(text->p);
        *text = str_from(r);
        free(r);
    }
}

static str_t replace_sentinel(str_t text) {
    sb_t out;
    size_t pos = 0, i = 0;
    if (find_bytes(text.p, text.n, 0, EQ_SENTINEL, EQ_LEN) < 0) return text;
    sb_init(&out);
    while (i + EQ_LEN <= text.n) {
        if (memcmp(text.p + i, EQ_SENTINEL, EQ_LEN) == 0) {
            sb_addn(&out, text.p + pos, i - pos);
            sb_adds(&out, "**");
            i += EQ_LEN;
            pos = i;
            continue;
        }
        i++;
    }
    sb_addn(&out, text.p + pos, text.n - pos);
    free(text.p);
    return sb_take(&out);
}

char *w31_telegramma_02bbc8_p1_to_markdown_v2(const char *content) {
    phstore_t ph;
    str_t text;

    memset(&ph, 0, sizeof ph);
    if (!content || !*content) return str_from("").p;

    text = str_from(content);

    /* 0) LaTeX macros in prose, then GFM pipe tables -> row groups (seams). */
    if (memchr(text.p, '\\', text.n)) run_hook(g_latex_hook, &text);
    run_hook(g_tables_hook, &text);

    /* 1) Protect fenced code blocks. */
    {
        str_t next = protect_fenced(text, &ph);
        free(text.p);
        text = next;
    }
    /* 2) Protect inline code. */
    {
        str_t next = stash_inline_code(text, &ph);
        free(text.p);
        text = next;
    }
    /* 3) Markdown links. */
    {
        str_t next = convert_links(text, &ph);
        free(text.p);
        text = next;
    }
    /* 3b) Bare URLs, escaped whole. */
    text = stash_bare_urls(text, &ph);
    /* 4) Headers. */
    {
        str_t next = convert_headers(text, &ph);
        free(text.p);
        text = next;
    }
    /* 4b) The expandable-quote opener's ``**`` is quote syntax, not a delimiter. */
    {
        str_t next = hide_expandable_quote(text);
        free(text.p);
        text = next;
    }
    /* 5) Bold and italic resolve together. */
    {
        str_t next = resolve_emphasis(text, &ph);
        free(text.p);
        text = next;
    }
    /* 6) Strikethrough; 7) spoiler. */
    {
        str_t next = stash_delimited(text, "~~", &ph, "~");
        free(text.p);
        text = next;
        next = stash_delimited(text, "||", &ph, "||");
        free(text.p);
        text = next;
    }
    /* 9) Blockquotes. */
    {
        str_t next = convert_blockquotes(text, &ph);
        free(text.p);
        text = next;
    }
    /* A sentinel left over means the opener had no quote text: restore its **. */
    text = replace_sentinel(text);
    /* 10) Escape the remaining special characters. */
    {
        str_t next = escape_mdv2_s(text);
        free(text.p);
        text = next;
    }
    /* 11) Restore placeholders in one pass, in insertion order. */
    for (int i = 0; i < ph.n; i++) {
        str_t v = expand_placeholders(ph.v[i], &ph, i);
        ph.v[i] = v;
    }
    text = expand_placeholders(text, &ph, ph.n);
    /* 12) Safety net: bare ( ) { } that slipped through, never inside code. */
    {
        str_t next = safety_net(text);
        free(text.p);
        text = next;
    }
    (void)text.n;
    for (int i = 0; i < ph.n; i++) free(ph.v[i].p);
    free(ph.v);
    return text.p;
}
