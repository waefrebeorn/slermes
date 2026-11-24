/*
 * telegram_latex.c - C11 port of upstream plugins/platforms/telegram/telegram_latex.py
 * (gate w26_telegramla_b660b0).
 *
 * PoP: w26_telegramla_b660b0_normalize @ plugins/platforms/telegram/telegram_latex.py:normalize_latex_macros
 * PoP: w26_telegramla_b660b0_replace_outside @ plugins/platforms/telegram/telegram_latex.py:replace_macros_outside
 * PoP: w26_telegramla_b660b0_constants @ plugins/platforms/telegram/telegram_latex.py:_MACROS
 *
 * Nothing here does I/O: the whole module is text in, text out.  The regexes are
 * ported as hand-written scanners with the same guards -- the path lookbehind,
 * the possessive macro run, the lazy inline-math body, and the URL spans that
 * are never rewritten.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_telegramla_b660b0.h"

typedef struct {
    const char *name;
    const char *sym;
} tl_macro;

/* _MACROS, in the alternation order the regex uses (longest name first). */
static const tl_macro TL_MACROS[] = {
    {"leftrightarrow", "\xe2\x86\x94"},
    {"Leftrightarrow", "\xe2\x87\x94"},
    {"longrightarrow", "\xe2\x9f\xb6"},
    {"Longrightarrow", "\xe2\x9f\xb9"},
    {"longleftarrow", "\xe2\x9f\xb5"},
    {"Longleftarrow", "\xe2\x9f\xb8"},
    {"rightarrow", "\xe2\x86\x92"},
    {"Rightarrow", "\xe2\x87\x92"},
    {"varnothing", "\xe2\x88\x85"},
    {"varepsilon", "\xce\xb5"},
    {"leftarrow", "\xe2\x86\x90"},
    {"Leftarrow", "\xe2\x87\x90"},
    {"downarrow", "\xe2\x86\x93"},
    {"checkmark", "\xe2\x9c\x93"},
    {"parallel", "\xe2\x88\xa5"},
    {"subseteq", "\xe2\x8a\x86"},
    {"supseteq", "\xe2\x8a\x87"},
    {"emptyset", "\xe2\x88\x85"},
    {"setminus", "\xe2\x88\x96"},
    {"vartheta", "\xcf\x91"},
    {"implies", "\xe2\x9f\xb9"},
    {"uparrow", "\xe2\x86\x91"},
    {"nearrow", "\xe2\x86\x97"},
    {"searrow", "\xe2\x86\x98"},
    {"nexists", "\xe2\x88\x84"},
    {"partial", "\xe2\x88\x82"},
    {"epsilon", "\xce\xb5"},
    {"upsilon", "\xcf\x85"},
    {"Upsilon", "\xce\xa5"},
    {"mapsto", "\xe2\x86\xa6"},
    {"approx", "\xe2\x89\x88"},
    {"propto", "\xe2\x88\x9d"},
    {"bullet", "\xe2\x80\xa2"},
    {"otimes", "\xe2\x8a\x97"},
    {"subset", "\xe2\x8a\x82"},
    {"supset", "\xe2\x8a\x83"},
    {"forall", "\xe2\x88\x80"},
    {"exists", "\xe2\x88\x83"},
    {"lambda", "\xce\xbb"},
    {"varphi", "\xcf\x86"},
    {"Lambda", "\xce\x9b"},
    {"langle", "\xe2\x9f\xa8"},
    {"rangle", "\xe2\x9f\xa9"},
    {"lfloor", "\xe2\x8c\x8a"},
    {"rfloor", "\xe2\x8c\x8b"},
    {"dagger", "\xe2\x80\xa0"},
    {"degree", "\xc2\xb0"},
    {"equiv", "\xe2\x89\xa1"},
    {"simeq", "\xe2\x89\x83"},
    {"times", "\xc3\x97"},
    {"oplus", "\xe2\x8a\x95"},
    {"notin", "\xe2\x88\x89"},
    {"wedge", "\xe2\x88\xa7"},
    {"infty", "\xe2\x88\x9e"},
    {"nabla", "\xe2\x88\x87"},
    {"alpha", "\xce\xb1"},
    {"gamma", "\xce\xb3"},
    {"delta", "\xce\xb4"},
    {"theta", "\xce\xb8"},
    {"kappa", "\xce\xba"},
    {"sigma", "\xcf\x83"},
    {"omega", "\xcf\x89"},
    {"Gamma", "\xce\x93"},
    {"Delta", "\xce\x94"},
    {"Theta", "\xce\x98"},
    {"Sigma", "\xce\xa3"},
    {"Omega", "\xce\xa9"},
    {"ldots", "\xe2\x80\xa6"},
    {"cdots", "\xe2\x8b\xaf"},
    {"lceil", "\xe2\x8c\x88"},
    {"rceil", "\xe2\x8c\x89"},
    {"angle", "\xe2\x88\xa0"},
    {"aleph", "\xe2\x84\xb5"},
    {"gets", "\xe2\x86\x90"},
    {"cong", "\xe2\x89\x85"},
    {"perp", "\xe2\x8a\xa5"},
    {"cdot", "\xc2\xb7"},
    {"circ", "\xe2\x88\x98"},
    {"star", "\xe2\x8b\x86"},
    {"land", "\xe2\x88\xa7"},
    {"lnot", "\xc2\xac"},
    {"prod", "\xe2\x88\x8f"},
    {"oint", "\xe2\x88\xae"},
    {"sqrt", "\xe2\x88\x9a"},
    {"beta", "\xce\xb2"},
    {"zeta", "\xce\xb6"},
    {"iota", "\xce\xb9"},
    {"dots", "\xe2\x80\xa6"},
    {"hbar", "\xe2\x84\x8f"},
    {"iff", "\xe2\x9f\xba"},
    {"leq", "\xe2\x89\xa4"},
    {"geq", "\xe2\x89\xa5"},
    {"neq", "\xe2\x89\xa0"},
    {"sim", "\xe2\x88\xbc"},
    {"div", "\xc3\xb7"},
    {"cup", "\xe2\x88\xaa"},
    {"cap", "\xe2\x88\xa9"},
    {"lor", "\xe2\x88\xa8"},
    {"vee", "\xe2\x88\xa8"},
    {"neg", "\xc2\xac"},
    {"top", "\xe2\x8a\xa4"},
    {"bot", "\xe2\x8a\xa5"},
    {"sum", "\xe2\x88\x91"},
    {"int", "\xe2\x88\xab"},
    {"eta", "\xce\xb7"},
    {"rho", "\xcf\x81"},
    {"tau", "\xcf\x84"},
    {"phi", "\xcf\x86"},
    {"chi", "\xcf\x87"},
    {"psi", "\xcf\x88"},
    {"Phi", "\xce\xa6"},
    {"Psi", "\xce\xa8"},
    {"ell", "\xe2\x84\x93"},
    {"to", "\xe2\x86\x92"},
    {"le", "\xe2\x89\xa4"},
    {"ge", "\xe2\x89\xa5"},
    {"ne", "\xe2\x89\xa0"},
    {"ll", "\xe2\x89\xaa"},
    {"gg", "\xe2\x89\xab"},
    {"pm", "\xc2\xb1"},
    {"mp", "\xe2\x88\x93"},
    {"in", "\xe2\x88\x88"},
    {"ni", "\xe2\x88\x8b"},
    {"mu", "\xce\xbc"},
    {"nu", "\xce\xbd"},
    {"xi", "\xce\xbe"},
    {"pi", "\xcf\x80"},
    {"Xi", "\xce\x9e"},
    {"Pi", "\xce\xa0"},
};

#define TL_MACRO_COUNT (sizeof(TL_MACROS) / sizeof(TL_MACROS[0]))

typedef struct {
    char *p;
    size_t len, cap;
} tl_sb;

static void tl_reserve(tl_sb *b, size_t extra) {
    size_t need = b->len + extra + 1;
    if (need > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < need) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
}

static void tl_putn(tl_sb *b, const char *s, size_t n) {
    tl_reserve(b, n);
    if (!b->p) return;
    if (n) memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void tl_puts(tl_sb *b, const char *s) { tl_putn(b, s ? s : "", strlen(s ? s : "")); }
static void tl_putc(tl_sb *b, char c) { tl_putn(b, &c, 1); }

static json_t *tl_take(tl_sb *b) {
    json_t *out;
    if (!b->p) return json_string("");
    out = json_string(b->p);
    free(b->p);
    b->p = NULL;
    return out;
}

/* Python str.strip() over ASCII whitespace. */
static char *tl_strip(const char *s) {
    size_t a = 0, b = strlen(s ? s : "");
    char *out;
    const char *t = s ? s : "";
    while (a < b && (t[a] == ' ' || t[a] == '\t' || t[a] == '\n' || t[a] == '\r' ||
                     t[a] == '\v' || t[a] == '\f')) a++;
    while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\t' || t[b - 1] == '\n' ||
                     t[b - 1] == '\r' || t[b - 1] == '\v' || t[b - 1] == '\f')) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, t + a, b - a);
    out[b - a] = '\0';
    return out;
}

static int tl_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int tl_wordc(char c) {
    return tl_alpha(c) || (c >= '0' && c <= '9') || c == '_';
}

static int tl_isdigit_c(char c) { return c >= '0' && c <= '9'; }

static int tl_ci_prefix(const char *s, const char *pfx) {
    size_t i = 0;
    while (pfx[i]) {
        char a = s[i], b = pfx[i];
        if (!a) return 0;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
        i++;
    }
    return 1;
}

/* The alternatives try (?:NAME)(?![A-Za-z]) in order; first success wins. */
static const tl_macro *tl_find(const char *p) {
    size_t i;
    for (i = 0; i < TL_MACRO_COUNT; i++) {
        size_t len = strlen(TL_MACROS[i].name);
        if (strncmp(p, TL_MACROS[i].name, len) == 0 && !tl_alpha(p[len])) return &TL_MACROS[i];
    }
    return NULL;
}

/* (?<![\w:/\.~%]) */
static int tl_lb_blocked(char c) {
    return tl_wordc(c) || c == ':' || c == '/' || c == '\\' || c == '.' || c == '~' || c == '%';
}

/* (?![A-Za-z-]|\\(?!(?:NAMES)(?![A-Za-z]))\w) */
static int tl_trail_blocks(const char *p) {
    if (tl_alpha(*p) || *p == '-') return 1;
    if (*p == '\\' && !tl_find(p + 1) && tl_wordc(p[1])) return 1;
    return 0;
}

/* _map_prose_macros */
static void tl_map_prose(const char *s, tl_sb *out) {
    size_t i = 0, n = strlen(s ? s : "");
    if (!s) return;
    while (i < n) {
        if (s[i] == '\\' && (i == 0 || !tl_lb_blocked(s[i - 1]))) {
            size_t p = i;
            int cnt = 0;
            for (;;) {
                const tl_macro *m;
                /* p must still be at a backslash: never read past the NUL. */
                if (s[p] != '\\') break;
                m = tl_find(s + p + 1);
                if (!m) break;
                p += 1 + strlen(m->name);
                cnt++;
            }
            if (cnt > 0 && !tl_trail_blocks(s + p)) {
                size_t q = i;
                while (q < p) {
                    const tl_macro *m = tl_find(s + q + 1);
                    if (!m) break;
                    tl_puts(out, m->sym);
                    q += 1 + strlen(m->name);
                }
                i = p;
                continue;
            }
        }
        tl_putc(out, s[i]);
        i++;
    }
}

/* _MATH_MACRO_RE: (?<!\\)\\(NAME)(?![A-Za-z]) */
static int tl_has_math_macro(const char *s) {
    size_t i = 0, n = strlen(s ? s : "");
    for (; i < n; i++)
        if (s[i] == '\\' && (i == 0 || s[i - 1] != '\\') && tl_find(s + i + 1)) return 1;
    return 0;
}

static void tl_math_sub(const char *s, tl_sb *out) {
    size_t i = 0, n = strlen(s ? s : "");
    while (i < n) {
        if (s[i] == '\\' && (i == 0 || s[i - 1] != '\\')) {
            const tl_macro *m = tl_find(s + i + 1);
            if (m) {
                tl_puts(out, m->sym);
                i += 1 + strlen(m->name);
                continue;
            }
        }
        tl_putc(out, s[i]);
        i++;
    }
}

/* _AMOUNT_AFTER_RE: [ \t]?\d matched at `pos`. */
static int tl_amount_after(const char *s, size_t pos) {
    const char *p = s + pos;
    if (*p == ' ' || *p == '\t') p++;
    return tl_isdigit_c(*p);
}

/* _INLINE_MATH_RE + _inline_math: (?<![\\$\w])\$([^$\n]{1,120}?)\$(?![\w$]) */
static void tl_inline_math(const char *s, tl_sb *out) {
    size_t i = 0, n = strlen(s ? s : "");
    while (i < n) {
        if (s[i] == '$' && (i == 0 || (!tl_wordc(s[i - 1]) && s[i - 1] != '$' && s[i - 1] != '\\'))) {
            size_t jj = 0, limit = i + 121;   /* body length 1..120 */
            int found = 0;
            size_t j;
            for (j = i + 2; j <= limit && j < n; j++) {
                /* The body may hold neither a newline nor a dollar. */
                if (s[j - 1] == '\n' || s[j - 1] == '$') break;
                if (s[j] == '$') {
                    char nx = s[j + 1];
                    /* The body may not hold a '$', so this is the only candidate
                     * closing dollar; a failing lookahead means no span here. */
                    if (nx && (tl_wordc(nx) || nx == '$')) break;
                    jj = j;
                    found = 1;
                    break;
                }
            }
            if (found) {
                char *body = (char *)malloc(jj - i);
                if (body) {
                    memcpy(body, s + i + 1, jj - i - 1);
                    body[jj - i - 1] = '\0';
                    if (tl_isdigit_c(body[0]) && tl_amount_after(s, jj + 1)) {
                        tl_putn(out, s + i, jj + 1 - i);         /* currency, not math */
                    } else if (!tl_has_math_macro(body)) {
                        tl_putn(out, s + i, jj + 1 - i);         /* real math we cannot render */
                    } else {
                        tl_sb conv = {NULL, 0, 0};
                        tl_math_sub(body, &conv);
                        if (conv.p && !strchr(conv.p, '\\') && !strchr(conv.p, '{')) {
                            char *st = tl_strip(conv.p);
                            tl_puts(out, st ? st : "");
                            free(st);
                        } else {
                            tl_putc(out, '$');
                            tl_puts(out, conv.p ? conv.p : "");
                            tl_putc(out, '$');
                        }
                        free(conv.p);
                    }
                    free(body);
                }
                i = jj + 1;
                continue;
            }
        }
        tl_putc(out, s[i]);
        i++;
    }
}

/* ---- _URL_RE ------------------------------------------------------------ */

static size_t tl_url_run(const char *s, size_t p) {
    /* [^\s<>]+ */
    size_t start = p;
    while (s[p] && s[p] != ' ' && s[p] != '\t' && s[p] != '\n' && s[p] != '\r' &&
           s[p] != '\v' && s[p] != '\f' && s[p] != '<' && s[p] != '>')
        p++;
    return p > start ? p : 0;
}

/* End index of the URL match starting at i, or i when nothing matches. */
static size_t tl_url_end(const char *s, size_t i) {
    size_t p;
    if (s[i] == ']' && s[i + 1] == '(') {
        /* \]\([^()\n]*(?:\([^()\n]*\)[^()\n]*)*\) */
        int depth = 0;
        for (p = i + 2; s[p]; p++) {
            if (s[p] == '\n') break;
            if (s[p] == '(') {
                if (depth == 1) break;
                depth = 1;
                continue;
            }
            if (s[p] == ')') {
                if (depth == 0) return p + 1;
                depth = 0;
            }
        }
        /* \]\([^)\n]*\) */
        for (p = i + 2; s[p] && s[p] != ')' && s[p] != '\n'; p++) {}
        if (s[p] == ')') return p + 1;
        return i;
    }
    /* [^\s<>]+ must hold at least one character AFTER the scheme. */
    if (tl_ci_prefix(s + i, "https://")) {
        p = tl_url_run(s, i + 8);
        return p ? p : i;
    }
    if (tl_ci_prefix(s + i, "http://")) {
        p = tl_url_run(s, i + 7);
        return p ? p : i;
    }
    if (tl_ci_prefix(s + i, "ftp://")) {
        p = tl_url_run(s, i + 6);
        return p ? p : i;
    }
    if (tl_ci_prefix(s + i, "www.")) {
        p = tl_url_run(s, i + 4);
        return p ? p : i;
    }
    return i;
}

typedef void (*tl_xf)(const char *, tl_sb *);

/* _outside_urls */
static void tl_outside_urls(const char *text, tl_xf xf, tl_sb *out) {
    size_t pos = 0, i = 0, n = strlen(text ? text : "");
    char *seg;
    if (!text) return;
    while (i < n) {
        size_t e = tl_url_end(text, i);
        if (e > i) {
            seg = (char *)malloc(i - pos + 1);
            if (seg) {
                memcpy(seg, text + pos, i - pos);
                seg[i - pos] = '\0';
                xf(seg, out);
                free(seg);
            }
            tl_putn(out, text + i, e - i);
            pos = e;
            i = e;
            continue;
        }
        i++;
    }
    seg = (char *)malloc(n - pos + 1);
    if (seg) {
        memcpy(seg, text + pos, n - pos);
        seg[n - pos] = '\0';
        xf(seg, out);
        free(seg);
    }
}

/* part -> _map_prose_macros(_INLINE_MATH_RE.sub(_inline_math, part)) */
static void tl_xf_prose(const char *seg, tl_sb *out) {
    tl_sb mid = {NULL, 0, 0};
    tl_inline_math(seg, &mid);
    tl_map_prose(mid.p ? mid.p : "", out);
    free(mid.p);
}

static void tl_xf_map_only(const char *seg, tl_sb *out) { tl_map_prose(seg, out); }

json_t *w26_telegramla_b660b0_normalize(const char *prose) {
    tl_sb out = {NULL, 0, 0};
    if (!prose) return json_string("");
    if (!strchr(prose, '\\')) return json_string(prose);
    tl_outside_urls(prose, tl_xf_prose, &out);
    return tl_take(&out);
}

json_t *w26_telegramla_b660b0_replace_outside(const char *prose, const json_t *kept_spans) {
    tl_sb out = {NULL, 0, 0};
    size_t pos = 0, i, n;
    size_t len;
    if (!prose) return json_string("");
    len = strlen(prose);
    n = (kept_spans && kept_spans->type == JSON_ARRAY) ? json_len(kept_spans) : 0;
    for (i = 0; i < n; i++) {
        const json_t *sp = json_get(kept_spans, i);
        size_t start, end;
        char *seg;
        if (!sp || sp->type != JSON_ARRAY || json_len(sp) < 2) continue;
        {
            const json_t *st = json_get(sp, 0);
            start = (size_t)(st && st->type == JSON_NUMBER ? st->num_val : 0.0);
        }
        {
            const json_t *e = json_get(sp, 1);
            end = (size_t)(e && e->type == JSON_NUMBER ? e->num_val : 0.0);
        }
        if (start < pos) continue;
        if (start > len) start = len;
        if (end > len) end = len;
        if (end < start) end = start;
        seg = (char *)malloc(start - pos + 1);
        if (seg) {
            memcpy(seg, prose + pos, start - pos);
            seg[start - pos] = '\0';
            if (strchr(seg, '\\')) tl_outside_urls(seg, tl_xf_map_only, &out);
            else tl_puts(&out, seg);
            free(seg);
        }
        tl_putn(&out, prose + start, end - start);
        pos = end;
    }
    {
        char *seg = (char *)malloc(len - pos + 1);
        if (seg) {
            memcpy(seg, prose + pos, len - pos);
            seg[len - pos] = '\0';
            if (strchr(seg, '\\')) tl_outside_urls(seg, tl_xf_map_only, &out);
            else tl_puts(&out, seg);
            free(seg);
        }
    }
    return tl_take(&out);
}

static const char *tl_sym(const char *name) {
    size_t i;
    for (i = 0; i < TL_MACRO_COUNT; i++)
        if (strcmp(TL_MACROS[i].name, name) == 0) return TL_MACROS[i].sym;
    return "";
}

json_t *w26_telegramla_b660b0_constants(void) {
    json_t *o = json_object();
    json_t *sample = json_object();
    json_set(o, "macro_count", json_number((double)TL_MACRO_COUNT));
    json_set(o, "inline_math_body_max", json_number(120));
    json_set(sample, "to", json_string(tl_sym("to")));
    json_set(sample, "leq", json_string(tl_sym("leq")));
    json_set(sample, "alpha", json_string(tl_sym("alpha")));
    json_set(sample, "leftrightarrow", json_string(tl_sym("leftrightarrow")));
    json_set(sample, "infty", json_string(tl_sym("infty")));
    json_set(o, "sample", sample);
    json_set(o, "ellipsis", json_string("\xe2\x80\xa6"));
    return o;
}
