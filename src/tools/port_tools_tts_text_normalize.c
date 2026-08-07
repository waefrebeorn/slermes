/*
 * port_tools_tts_text_normalize.c — Faithful C11 port of tools/tts_text_normalize.py
 *
 * Pure deterministic text transformations for speech synthesis. No I/O, no
 * network — just string processing. PCRE2 is the regex engine (slermes links
 * -lpcre2-8): Python's `re` is PCRE-compatible here — lookbehind/ahead,
 * Unicode emoji ranges, DOTALL and IGNORECASE all reproduce byte-for-byte.
 *
 * Internal substitution helpers are NUL- and length-aware so the heading
 * sentinel byte (\x00, Python's _HEAD) survives every pass and is only
 * removed by smooth_whitespace_for_tts as Python does.
 *
 * PoP-annotated for the parity scanner. Verified byte-equal to LIVE Python
 * via the tools_tts_text_normalize oracle harness (tests/t_port_* pair).
 */

#define PCRE2_CODE_UNIT_WIDTH 8

#include "port_tools_tts_text_normalize.h"

#include <pcre2.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "html.h"   /* html_unescape() — mirrors Python's html.unescape */

/* ------------------------------------------------------------------ */
/* Module-level regex bindings (mirror Python's _MD_* / _EMOJI_* etc) */
/* ------------------------------------------------------------------ */

/* Sentinel appended to former heading lines so smooth_whitespace_for_tts can
 * fold a heading into the sentence that follows it (Python _HEAD = "\x00").
 * The real NUL byte is used so standalone/pipeline outputs are byte-identical
 * to Python. Internal buffers are length-tracked; the JSON emitter encodes
 * NUL as \u0000. */
#define TTS_HEAD  "\x00"

#define _MD_CODE_BLOCK_RE       "```[\\s\\S]*?```"
#define _MD_LINK_RE             "\\[([^\\]]+)\\]\\((?:[^()]|\\([^)]*\\))*\\)"
#define _MD_IMAGE_RE            "!\\[([^\\]]*)\\]\\((?:[^()]|\\([^)]*\\))*\\)"
#define _MD_INLINE_CODE_RE      "`([^`]+)`"
#define _MD_BOLD_RE             "\\*\\*(.+?)\\*\\*"
#define _MD_UNDERSCORE_BOLD_RE  "__(.+?)__"
#define _MD_ITALIC_RE           "(?<!\\*)\\*(?!\\*)(.+?)(?<!\\*)\\*(?!\\*)"
#define _MD_UNDERSCORE_ITALIC_RE "(?<!_)_(?!_)(.+?)(?<!_)_(?!_)"
#define _MD_STRIKE_RE           "~~(.+?)~~"
#define _MD_HEADING_LINE_RE     "^[ \\t]{0,3}#{1,6}[ \\t]+(.+?)[ \\t]*#*[ \\t]*$"
#define _MD_BLOCKQUOTE_RE       "^\\s*>\\s?"
#define _MD_LIST_ITEM_RE        "^\\s*(?:[-*+]|\\d+[.)])\\s+"
#define _MD_HR_RE               "^\\s*[-*_]{3,}\\s*$"
#define _MD_TABLE_PIPE_RE       "\\s*\\|\\s*"
#define _URL_RE                 "https?://\\S+"

/* Unicode via PCRE2 \x{} syntax (slermes builds with UTF|UCP). */
#define _MINUS_SIGN_RE          "\\x{2212}"
#define _ELLIPSIS_RE            "\\x{2026}"
#define _SPECIAL_SPACES_RE      "[ \\x{A0}\\x{2007}\\x{202F}]"
#define _DEGREE_RE              "\\x{B0}"
#define _POUND_RE               "\\x{A3}"
#define _EURO_RE                "\\x{20AC}"
#define _BULLET_RE              "[\\x{2022}\\x{25E6}\\x{25AA}\\x{25AB}]"
#define _VAR_SELECTOR_RE        "[\\x{FE0E}\\x{FE0F}]"
#define _EMOJI_RE               "[\\x{1F1E6}-\\x{1F1FF}\\x{1F300}-\\x{1F5FF}\\x{1F600}-\\x{1F64F}\\x{1F680}-\\x{1F6FF}\\x{1F700}-\\x{1F77F}\\x{1F780}-\\x{1F7FF}\\x{1F800}-\\x{1F8FF}\\x{1F900}-\\x{1F9FF}\\x{1FA00}-\\x{1FAFF}\\x{2600}-\\x{27BF}]+"
#define _THINK_BLOCK_RE         "<think[\\s>].*?</think>"
#define _THINK_BLOCK_OPEN_RE    "<think[\\s>].*\\z"
#define _VERIFIER_FOOTER_RE     "^\\s*\\x{26A0}\\x{FE0F}?\\s*File-mutation verifier:.*(?:\\n[ \\t]+\\x{2022}.*)*"

/* ------------------------------------------------------------------ */
/* Tiny dynabuffer (append-only, caller frees)                        */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *buf;
    size_t len;     /* bytes used (NUL not counted) */
    size_t cap;
} tts_buf_t;

static void tts_buf_init(tts_buf_t *b)
{
    b->cap = 256; b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (!b->buf) abort();
    b->buf[0] = '\0';
}

static void tts_buf_reserve(tts_buf_t *b, size_t need)
{
    if (b->len + need + 1 <= b->cap) return;
    size_t n = b->cap * 2;
    while (n < b->len + need + 1) n *= 2;
    char *nb = (char *)realloc(b->buf, n);
    if (!nb) abort();
    b->buf = nb; b->cap = n;
}

static void tts_buf_put(tts_buf_t *b, const char *s, size_t n)
{
    tts_buf_reserve(b, n);
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void tts_buf_putc(tts_buf_t *b, char c)
{
    tts_buf_reserve(b, 1);
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

static void tts_buf_puts(tts_buf_t *b, const char *s)
{
    tts_buf_put(b, s, strlen(s));
}

/* ------------------------------------------------------------------ */
/* Python-whitespace helpers (str.strip / str.rstrip semantics)        */
/* ------------------------------------------------------------------ */

/* Python str.isspace()/str.strip() strips Unicode White_Space — not just
 * ASCII. Returns true (and sets *adv to the code point's byte length) when
 * s begins with a Python-whitespace code point. */
static bool tts_py_ws(const char *s, int *adv)
{
    unsigned char c = (unsigned char)s[0];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
        c == '\v' || c == '\f' || c == 0x85) { *adv = 1; return true; }
    if (c == 0xC2) {
        unsigned char b = (unsigned char)s[1];
        if (b == 0xA0 || b == 0x85) { *adv = 2; return true; }
    }
    if (c == 0xE1 && (unsigned char)s[1] == 0x9A &&
        (unsigned char)s[2] == 0x80) { *adv = 3; return true; }   /* U+1680 */
    if (c == 0xE2 && (unsigned char)s[1] == 0x80) {
        unsigned char b = (unsigned char)s[2];
        if ((b >= 0x80 && b <= 0x8A) || b == 0xA8 || b == 0xA9 ||
            b == 0xAF) { *adv = 3; return true; }
    }
    if (c == 0xE2 && (unsigned char)s[1] == 0x81 &&
        (unsigned char)s[2] == 0x9F) { *adv = 3; return true; }   /* U+205F */
    if (c == 0xE3 && (unsigned char)s[1] == 0x80 &&
        (unsigned char)s[2] == 0x80) { *adv = 3; return true; }   /* U+3000 */
    *adv = 1;
    return false;
}

/* Trim Python-whitespace from both ends of byte span [s, s+n). Never splits a
 * UTF-8 sequence. Output span [ost, oen). */
static void tts_py_strip_span(const char *s, size_t n, size_t *ost, size_t *oen)
{
    size_t st = 0;
    while (st < n) {
        int adv;
        if (!tts_py_ws(s + st, &adv)) break;
        st += adv;
    }
    size_t en = n;
    while (en > st) {
        size_t cs = en - 1;
        while (cs > st && ((unsigned char)s[cs] & 0xC0) == 0x80) cs--;
        int adv;
        if (!tts_py_ws(s + cs, &adv)) break;
        en = cs;
    }
    *ost = st;
    *oen = en;
}

/* ------------------------------------------------------------------ */
/* PCRE2 helpers (length-aware throughout)                            */
/* ------------------------------------------------------------------ */

static pcre2_code *tts_compile(const char *pat, uint32_t extra)
{
    int err; PCRE2_SIZE erroff;
    pcre2_code *re = pcre2_compile(
        (PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
        PCRE2_UTF | PCRE2_UCP | extra, &err, &erroff, NULL);
    if (!re) {
        char buf[256];
        pcre2_get_error_message(err, (PCRE2_UCHAR *)buf, sizeof(buf));
        fprintf(stderr,
                "tts_text_normalize: pcre2_compile failed at %zu: %s [%s]\n",
                (size_t)erroff, buf, pat);
        abort();
    }
    return re;
}

/* Extract substring text[so..eo) (PCRE2 0-based byte offsets). */
static char *tts_substr(const char *text, PCRE2_SIZE so, PCRE2_SIZE eo)
{
    if (so == PCRE2_UNSET || eo == PCRE2_UNSET || so > eo)
        return strdup("");
    size_t n = (size_t)(eo - so);
    char *s = (char *)malloc(n + 1);
    if (!s) abort();
    memcpy(s, text + so, n);
    s[n] = '\0';
    return s;
}

/* Scratch buffer handed to replacement callbacks. `len` tracks bytes written
 * (NUL-aware — callbacks may emit the heading sentinel \x00). */
typedef struct {
    char  *dst;
    size_t len;     /* bytes written (NUL not counted) */
    size_t cap;
} tts_scratch_t;

typedef void (*tts_repl_fn)(const char *text, PCRE2_SIZE *ov, int ngroup,
                            tts_scratch_t *sk, void *ctx);

static void tts_scratch_ensure(tts_scratch_t *sk, size_t need)
{
    if (need <= sk->cap) return;
    size_t n = sk->cap * 2;
    while (n < need) n *= 2;
    char *nb = (char *)realloc(sk->dst, n);
    if (!nb) abort();
    sk->dst = nb; sk->cap = n;
}

static void tts_scratch_put(tts_scratch_t *sk, const char *s, size_t n)
{
    tts_scratch_ensure(sk, sk->len + n);
    memcpy(sk->dst + sk->len, s, n);
    sk->len += n;
    sk->dst[sk->len] = '\0';
}

static void tts_scratch_puts(tts_scratch_t *sk, const char *s)
{
    tts_scratch_put(sk, s, strlen(s));
}

static void tts_scratch_putc(tts_scratch_t *sk, char c)
{
    tts_scratch_ensure(sk, sk->len + 1);
    sk->dst[sk->len++] = c;
    sk->dst[sk->len] = '\0';
}

/* Callback-driven substitution mirroring re.sub(pattern, lambda m: repl(m),
 * text). NUL/length-aware: threads `tlen` so embedded NULs (the heading
 * sentinel) survive. Emits result into *out (caller frees) with *out_len. */
static char *tts_pcre2_subn(const char *pattern, tts_repl_fn repl, void *ctx,
                            const char *text, size_t tlen, uint32_t extra,
                            char **out, size_t *out_len)
{
    pcre2_code *re = tts_compile(pattern, extra);
    tts_buf_t ob; tts_buf_init(&ob);

    PCRE2_SIZE offset = 0;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);

    while (1) {
        int rc = pcre2_match(re, (PCRE2_SPTR)text, tlen, offset, 0, md, NULL);
        if (rc < 0) break;

        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        PCRE2_SIZE mstart = ov[0];
        PCRE2_SIZE mend   = ov[1];
        int ngroup = (rc > 0) ? rc - 1 : 0;

        tts_buf_put(&ob, text + offset, mstart - offset);

        tts_scratch_t sk;
        sk.cap = 256; sk.len = 0;
        sk.dst = (char *)malloc(sk.cap);
        if (!sk.dst) abort();
        sk.dst[0] = '\0';
        repl(text, ov, ngroup, &sk, ctx);
        tts_buf_put(&ob, sk.dst, sk.len);
        free(sk.dst);

        if (mend > mstart) {
            offset = mend;
        } else {
            /* Zero-width match: Python's re.sub copies the single byte after
             * the empty match and resumes one position later. */
            if (mstart >= tlen) break;
            tts_buf_put(&ob, text + mstart, 1);
            offset = mstart + 1;
        }
    }
    tts_buf_put(&ob, text + offset, tlen - offset);

    pcre2_match_data_free(md);
    pcre2_code_free(re);
    *out = ob.buf;
    *out_len = ob.len;
    return ob.buf;
}

/* String-template substitution mirroring re.sub(pattern, repl, text) where
 * `repl` may contain backrefs \1..\9. NUL/length-aware. */
static char *tts_pcre2_sub(const char *pattern, const char *repl,
                           const char *text, size_t tlen, uint32_t extra,
                           char **out, size_t *out_len)
{
    pcre2_code *re = tts_compile(pattern, extra);
    tts_buf_t ob; tts_buf_init(&ob);

    PCRE2_SIZE offset = 0;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);

    while (1) {
        int rc = pcre2_match(re, (PCRE2_SPTR)text, tlen, offset, 0, md, NULL);
        if (rc < 0) break;

        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
        PCRE2_SIZE mstart = ov[0];
        PCRE2_SIZE mend   = ov[1];

        tts_buf_put(&ob, text + offset, mstart - offset);

        /* Process the template: \N -> group N; everything else literal. */
        const char *tp = repl ? repl : "";
        while (*tp) {
            if (*tp == '\\' && tp[1] >= '1' && tp[1] <= '9') {
                int g = tp[1] - '0';
                PCRE2_SIZE so = ov[2 * g];
                PCRE2_SIZE eo = ov[2 * g + 1];
                if (so != PCRE2_UNSET && eo != PCRE2_UNSET && eo >= so) {
                    char *cap = tts_substr(text, so, eo);
                    size_t cl = strlen(cap);
                    tts_buf_put(&ob, cap, cl);
                    free(cap);
                }
                tp += 2;
            } else {
                tts_buf_putc(&ob, *tp);
                tp++;
            }
        }

        if (mend > mstart) {
            offset = mend;
        } else {
            if (mstart >= tlen) break;
            tts_buf_put(&ob, text + mstart, 1);
            offset = mstart + 1;
        }
    }
    tts_buf_put(&ob, text + offset, tlen - offset);

    pcre2_match_data_free(md);
    pcre2_code_free(re);
    *out = ob.buf;
    *out_len = ob.len;
    return ob.buf;
}

/* Convenience wrappers for NUL-terminated inputs that never contain NUL. */
static char *tts_sub(const char *pat, const char *repl, const char *text,
                     uint32_t extra, char **out, size_t *out_len)
{
    return tts_pcre2_sub(pat, repl, text, strlen(text), extra, out, out_len);
}
static char *tts_sub_fn(const char *pat, tts_repl_fn fn, void *ctx,
                        const char *text, uint32_t extra,
                        char **out, size_t *out_len)
{
    return tts_pcre2_subn(pat, fn, ctx, text, strlen(text), extra, out, out_len);
}

/* Length-aware cores used by prepare_spoken_text (defined later in file). */
static char *tts_normalize_symbols_len(const char *text, size_t tlen,
                                       size_t *out_len);
static char *tts_smooth_whitespace_len(const char *text, size_t total,
                                       size_t *out_len);

/* ------------------------------------------------------------------ */
/* strip_markdown_for_tts                                              */
/* ------------------------------------------------------------------ */

/* Python: _MD_IMAGE_RE.sub(lambda m: f" {m.group(1)} " if m.group(1) else " ") */
static void tts_repl_image(const char *text, PCRE2_SIZE *ov, int ngroup,
                           tts_scratch_t *sk, void *ctx)
{
    (void)ctx; (void)ngroup;
    char *cap = tts_substr(text, ov[2], ov[3]);   /* group 1 = alt text */
    if (cap[0] != '\0') {
        tts_scratch_putc(sk, ' ');
        tts_scratch_puts(sk, cap);
        tts_scratch_putc(sk, ' ');
    } else {
        tts_scratch_putc(sk, ' ');
    }
    free(cap);
}

/* Python: _MD_HEADING_LINE_RE.sub(lambda m: m.group(1).rstrip() + _HEAD) */
static void tts_repl_heading(const char *text, PCRE2_SIZE *ov, int ngroup,
                             tts_scratch_t *sk, void *ctx)
{
    (void)ctx; (void)ngroup;
    char *cap = tts_substr(text, ov[2], ov[3]);   /* group 1 = heading body */
    size_t ost, oen;
    tts_py_strip_span(cap, strlen(cap), &ost, &oen);
    tts_scratch_put(sk, cap + ost, oen - ost);   /* str.rstrip() */
    tts_scratch_putc(sk, TTS_HEAD[0]);           /* + _HEAD (NUL) */
    free(cap);
}

/* PoP: strip_markdown_for_tts @ tools/tts_text_normalize.py:strip_markdown_for_tts */
char *tts_strip_markdown(const char *text, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!text || text[0] == '\0') { char *e = strdup(""); if (out_len) *out_len = 0; return e; }

    char *unescaped = html_unescape(text);
    if (!unescaped) unescaped = strdup(text);
    size_t ulen = strlen(unescaped);

    char *cur = NULL; size_t cl = 0;
    char *nxt = NULL; size_t nl = 0;

    /* Each step: free cur, then cur = result of substituting on cur. */
    tts_pcre2_sub(_MD_CODE_BLOCK_RE, " ", unescaped, ulen, PCRE2_DOTALL, &cur, &cl); free(unescaped);
    tts_pcre2_subn(_MD_IMAGE_RE, tts_repl_image, NULL, cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_LINK_RE, "\\1", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_URL_RE, "", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_INLINE_CODE_RE, "\\1", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_BOLD_RE, "\\1", cur, cl, PCRE2_DOTALL, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_UNDERSCORE_BOLD_RE, "\\1", cur, cl, PCRE2_DOTALL, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_ITALIC_RE, "\\1", cur, cl, PCRE2_DOTALL, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_UNDERSCORE_ITALIC_RE, "\\1", cur, cl, PCRE2_DOTALL, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_STRIKE_RE, "\\1", cur, cl, PCRE2_DOTALL, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    /* Headings -> body + TTS_HEAD sentinel (NUL). Length-aware so the NUL
     * survives into the return value (matches Python's \x00). */
    tts_pcre2_subn(_MD_HEADING_LINE_RE, tts_repl_heading, NULL,
                   cur, cl, PCRE2_MULTILINE, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_BLOCKQUOTE_RE, "", cur, cl, PCRE2_MULTILINE, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_LIST_ITEM_RE, "", cur, cl, PCRE2_MULTILINE, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_HR_RE, "", cur, cl, PCRE2_MULTILINE, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_MD_TABLE_PIPE_RE, "; ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    if (out_len) *out_len = cl;
    return cur;
}

/* ------------------------------------------------------------------ */
/* normalize_symbols_for_tts                                           */
/* ------------------------------------------------------------------ */

/* Replace every U+2212 (E2 88 92) with a single hyphen, mirroring Python's
 * str.replace(chr(0x2212), "-"). Returns a fresh string. */
static char *tts_minus_to_hyphen(const char *s)
{
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) abort();
    size_t w = 0;
    for (size_t i = 0; i < n; ) {
        if (i + 2 < n && (unsigned char)s[i] == 0xE2 &&
            (unsigned char)s[i+1] == 0x88 && (unsigned char)s[i+2] == 0x92) {
            r[w++] = '-';
            i += 3;
        } else {
            r[w++] = s[i++];
        }
    }
    r[w] = '\0';
    return r;
}

/* _normalize_temperature_ranges: "11–17 °C" -> "11 to 17 degrees Celsius".
 * Python rewrites the U+2212 minus inside each captured number to hyphen. */
static void tts_repl_temp_c(const char *text, PCRE2_SIZE *ov, int ngroup,
                            tts_scratch_t *sk, void *ctx)
{
    (void)ctx; (void)ngroup;
    char *g1 = tts_substr(text, ov[2], ov[3]);
    char *g2 = tts_substr(text, ov[4], ov[5]);
    char *h1 = tts_minus_to_hyphen(g1);
    char *h2 = tts_minus_to_hyphen(g2);
    tts_scratch_puts(sk, h1);
    tts_scratch_puts(sk, " to ");
    tts_scratch_puts(sk, h2);
    tts_scratch_puts(sk, " degrees Celsius");
    free(g1); free(g2); free(h1); free(h2);
}

static void tts_repl_temp_f(const char *text, PCRE2_SIZE *ov, int ngroup,
                            tts_scratch_t *sk, void *ctx)
{
    (void)ctx; (void)ngroup;
    char *g1 = tts_substr(text, ov[2], ov[3]);
    char *g2 = tts_substr(text, ov[4], ov[5]);
    char *h1 = tts_minus_to_hyphen(g1);
    char *h2 = tts_minus_to_hyphen(g2);
    tts_scratch_puts(sk, h1);
    tts_scratch_puts(sk, " to ");
    tts_scratch_puts(sk, h2);
    tts_scratch_puts(sk, " degrees Fahrenheit");
    free(g1); free(g2); free(h1); free(h2);
}

/* PoP: normalize_symbols_for_tts @ tools/tts_text_normalize.py:normalize_symbols_for_tts */
char *tts_normalize_symbols(const char *text, size_t *out_len)
{
    return tts_normalize_symbols_len(text, text ? strlen(text) : 0, out_len);
}

/* Length-aware core: `text` may carry the embedded-NUL heading sentinel from
 * strip_markdown_for_tts (which Python's str handles natively), so the input
 * length is explicit instead of strlen(). */
static char *tts_normalize_symbols_len(const char *text, size_t tlen, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!text || tlen == 0) { if (out_len) *out_len = 0; return strdup(""); }

    char *cur = NULL; size_t cl = 0;
    char *nxt = NULL; size_t nl = 0;

    tts_pcre2_sub(_SPECIAL_SPACES_RE, " ", text, tlen, 0, &cur, &cl);
    tts_pcre2_sub(_MINUS_SIGN_RE, "-", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_ELLIPSIS_RE, "...", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    tts_pcre2_subn(
        "(?<!\\w)([-+\\x{2212}]?\\d+(?:\\.\\d+)?)\\s*[\\x{2013}\\x{2014}-]\\s*([-+\\x{2212}]?\\d+(?:\\.\\d+)?)\\s*\\x{B0}\\s*C\\b",
        tts_repl_temp_c, NULL, cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_subn(
        "(?<!\\w)([-+\\x{2212}]?\\d+(?:\\.\\d+)?)\\s*[\\x{2013}\\x{2014}-]\\s*([-+\\x{2212}]?\\d+(?:\\.\\d+)?)\\s*\\x{B0}\\s*F\\b",
        tts_repl_temp_f, NULL, cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    tts_pcre2_sub("(?<!\\w)([-+]?\\d+(?:\\.\\d+)?)\\s*\\x{B0}\\s*C\\b",
                  "\\1 degrees Celsius", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("(?<!\\w)([-+]?\\d+(?:\\.\\d+)?)\\s*\\x{B0}\\s*F\\b",
                  "\\1 degrees Fahrenheit", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\x{B0}\\s*C\\b", "degrees Celsius", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\x{B0}\\s*F\\b", "degrees Fahrenheit", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("(?<!\\w)([-+]?\\d+(?:\\.\\d+)?)\\s*\\x{B0}",
                  "\\1 degrees", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_DEGREE_RE, " degrees", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    tts_pcre2_sub("(?<=\\d)\\s*km\\s*/\\s*h\\b", " kilometres per hour", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("(?<=\\d)\\s*km/h\\b", " kilometres per hour", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("(?<=\\d)\\s*mm\\b", " millimetres", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("(?<=\\d)\\s*cm\\b", " centimetres", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("(?<=\\d)\\s*m\\b", " metres", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    tts_pcre2_sub("(?<=\\d)\\s*/\\s*(?=[A-Za-z])", " per ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    tts_pcre2_sub("NZ\\$\\s*([\\d,]*\\d(?:\\.\\d+)?)", "\\1 New Zealand dollars", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("A\\$\\s*([\\d,]*\\d(?:\\.\\d+)?)", "\\1 Australian dollars", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("US\\$\\s*([\\d,]*\\d(?:\\.\\d+)?)", "\\1 US dollars", cur, cl, PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_EURO_RE "\\s*([\\d,]*\\d(?:\\.\\d+)?)", "\\1 euros", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_POUND_RE "\\s*([\\d,]*\\d(?:\\.\\d+)?)", "\\1 pounds", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\$\\s*([\\d,]*\\d(?:\\.\\d+)?)", "\\1 dollars", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("(?<=\\d)\\s*%", " percent", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    tts_pcre2_sub("&", " and ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_BULLET_RE, " ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\x{2192}", " to ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\x{21D2}", " to ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\x{2248}", " about ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("~", " about ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    tts_pcre2_sub(_VAR_SELECTOR_RE, "", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_EMOJI_RE, "", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    if (out_len) *out_len = cl;
    return cur;
}

/* ------------------------------------------------------------------ */
/* strip_nonspoken_blocks                                              */
/* ------------------------------------------------------------------ */

/* PoP: strip_nonspoken_blocks @ tools/tts_text_normalize.py:strip_nonspoken_blocks */
char *tts_strip_nonspoken_blocks(const char *text, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!text || text[0] == '\0') { if (out_len) *out_len = 0; return strdup(""); }

    size_t tlen = strlen(text);
    char *cur = NULL, *nxt = NULL;
    size_t cl = 0, nl = 0;
    tts_pcre2_sub(_THINK_BLOCK_RE, " ", text, tlen, PCRE2_DOTALL | PCRE2_CASELESS, &cur, &cl);
    tts_pcre2_sub(_THINK_BLOCK_OPEN_RE, " ", cur, cl, PCRE2_DOTALL | PCRE2_CASELESS, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub(_VERIFIER_FOOTER_RE, " ", cur, cl, PCRE2_MULTILINE, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    if (out_len) *out_len = cl;
    return cur;
}

/* ------------------------------------------------------------------ */
/* flatten_newlines_for_payload                                        */
/* ------------------------------------------------------------------ */

/* PoP: flatten_newlines_for_payload @ tools/tts_text_normalize.py:flatten_newlines_for_payload */
char *tts_flatten_newlines(const char *text, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!text || text[0] == '\0') { if (out_len) *out_len = 0; return strdup(""); }

    char *cur = NULL, *nxt = NULL;
    size_t cl = 0, nl = 0;
    tts_pcre2_sub("\\n{2,}", ". ", text, strlen(text), 0, &cur, &cl);
    tts_pcre2_sub("(?<=[.!?;:,])\\n", " ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\n", ". ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\.\\s*\\.", ".", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("[ \\t]{2,}", " ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    /* .strip() — Python whitespace semantics. */
    size_t ost, oen;
    tts_py_strip_span(cur, cl, &ost, &oen);
    size_t n = oen - ost;
    char *res = (char *)malloc(n + 1);
    if (!res) abort();
    memcpy(res, cur + ost, n);
    res[n] = '\0';
    free(cur);
    if (out_len) *out_len = n;
    return res;
}

/* ------------------------------------------------------------------ */
/* smooth_whitespace_for_tts                                           */
/* ------------------------------------------------------------------ */

/* rstrip(".:;," ) — like Python str.rstrip(chars) from the right only. */
static size_t tts_rstrip_chars(const char *s, size_t n)
{
    while (n > 0) {
        char c = s[n-1];
        if (c=='.'||c==':'||c==';'||c==',') n--; else break;
    }
    return n;
}

/* PoP: smooth_whitespace_for_tts @ tools/tts_text_normalize.py:smooth_whitespace_for_tts */
char *tts_smooth_whitespace(const char *text, size_t *out_len)
{
    return tts_smooth_whitespace_len(text, text ? strlen(text) : 0, out_len);
}

/* Length-aware core: input may still carry the embedded-NUL heading sentinel
 * from strip_markdown_for_tts (Python's str handles NULs natively). */
static char *tts_smooth_whitespace_len(const char *text, size_t total,
                                       size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!text || total == 0) { if (out_len) *out_len = 0; return strdup(""); }

    /* text.splitlines(): split on \n; a trailing \r (from \r\n) is dropped. */
    typedef struct { const char *p; size_t n; } tts_line_t;
    tts_line_t *lns = NULL;
    size_t nln = 0, cap = 0;

    const char *p = text;
    while (p <= text + total) {
        if (nln == cap) {
            cap = cap ? cap * 2 : 64;
            tts_line_t *nl = (tts_line_t *)realloc(lns, cap * sizeof(tts_line_t));
            if (!nl) abort();
            lns = nl;
        }
        const char *e = memchr(p, '\n', (size_t)(text + total - p));
        size_t n = e ? (size_t)(e - p) : (size_t)(text + total - p);
        if (n > 0 && p[n-1] == '\r') n--;
        lns[nln].p = p; lns[nln].n = n; nln++;
        if (!e) break;
        p = e + 1;
    }

    /* add_sentence_pauses = sum(1 for raw_line in raw_lines
     *   if raw_line.replace(_HEAD,"").strip()) > 1.
     * A line that is ONLY the sentinel becomes "" after replace+strip. */
    int content_lines = 0;
    for (size_t i = 0; i < nln; i++) {
        const char *lp = lns[i].p; size_t ln = lns[i].n;
        tts_buf_t nb; tts_buf_init(&nb);
        for (size_t k = 0; k < ln; k++)
            if ((unsigned char)lp[k] != (unsigned char)TTS_HEAD[0])
                tts_buf_putc(&nb, lp[k]);
        size_t ost, oen;
        tts_py_strip_span(nb.buf, nb.len, &ost, &oen);
        if (oen > ost) content_lines++;
        free(nb.buf);
    }
    bool add_sentence_pauses = content_lines > 1;

    /* Second pass: build Python's `lines` list, join with "\n". */
    char **lines = NULL;
    size_t nlines = 0, lcap = 0;
    char *pending_heading = NULL;
    bool have_pending = false;

#define LINES_PUSH(s) do { \
        if (nlines == lcap) { \
            lcap = lcap ? lcap * 2 : 64; \
            char **l2 = (char **)realloc(lines, lcap * sizeof(char *)); \
            if (!l2) abort(); \
            lines = l2; \
        } \
        lines[nlines++] = (s); \
    } while (0)

#define FREE_LINE(p) do { free(p); } while (0)

#define FLUSH_PENDING() do { \
        if (have_pending) { \
            size_t hlen = tts_rstrip_chars(pending_heading, strlen(pending_heading)); \
            char *h = (char *)malloc(hlen + 2); \
            if (!h) abort(); \
            memcpy(h, pending_heading, hlen); \
            h[hlen] = '.'; h[hlen+1] = '\0'; \
            LINES_PUSH(h); \
            free(pending_heading); \
            pending_heading = NULL; \
            have_pending = false; \
        } \
    } while (0)

    for (size_t i = 0; i < nln; i++) {
        const char *raw = lns[i].p; size_t rawn = lns[i].n;

        /* is_heading = raw_line.rstrip().endswith(_HEAD) */
        size_t ost0, oen0;
        tts_py_strip_span(raw, rawn, &ost0, &oen0);
        bool is_heading = (oen0 > ost0 &&
            (unsigned char)raw[oen0-1] == (unsigned char)TTS_HEAD[0]);

        /* line = raw_line.replace(_HEAD, "").strip() */
        tts_buf_t lb; tts_buf_init(&lb);
        char *line = NULL;
        if (rawn == 0) {
            line = strdup("");
        } else {
            for (size_t k = 0; k < rawn; k++)
                if ((unsigned char)raw[k] != (unsigned char)TTS_HEAD[0])
                    tts_buf_putc(&lb, raw[k]);
            size_t lsp, lep;
            tts_py_strip_span(lb.buf, lb.len, &lsp, &lep);
            size_t m = lep - lsp;
            line = (char *)malloc(m + 1);
            if (!line) abort();
            memcpy(line, lb.buf + lsp, m);
            line[m] = '\0';
        }
        free(lb.buf);

        if (line[0] == '\0') {
            /* Hold a pending heading across blanks; else collapse blank. */
            if (!have_pending && nlines > 0 && lines[nlines-1][0] != '\0')
                LINES_PUSH(strdup(""));
            FREE_LINE(line);
            continue;
        }

        if (is_heading) {
            FLUSH_PENDING();
            size_t hlen = tts_rstrip_chars(line, strlen(line));
            free(pending_heading);
            pending_heading = (char *)malloc(hlen + 1);
            if (!pending_heading) abort();
            memcpy(pending_heading, line, hlen);
            pending_heading[hlen] = '\0';
            have_pending = true;
            FREE_LINE(line);
            continue;
        }

        if (have_pending) {
            size_t ph = tts_rstrip_chars(pending_heading, strlen(pending_heading));
            size_t ll = strlen(line);
            char *tmp = (char *)malloc(ph + 2 + ll + 1);
            if (!tmp) abort();
            memcpy(tmp, pending_heading, ph);
            tmp[ph] = ','; tmp[ph+1] = ' ';
            memcpy(tmp + ph + 2, line, ll);
            tmp[ph + 2 + ll] = '\0';
            FREE_LINE(line);
            line = tmp;
            free(pending_heading);
            pending_heading = NULL;
            have_pending = false;
        }

        if (add_sentence_pauses) {
            size_t L = strlen(line);
            if (L > 0 && !strchr(".!?;:", line[L-1])) {
                char *nl = (char *)malloc(L + 2);
                if (!nl) abort();
                memcpy(nl, line, L);
                nl[L] = '.'; nl[L+1] = '\0';
                FREE_LINE(line);
                line = nl;
            }
        }
        LINES_PUSH(line);
    }
    FLUSH_PENDING();

    /* "\n".join(lines) then the trailing re.sub passes. */
    tts_buf_t joined; tts_buf_init(&joined);
    for (size_t i = 0; i < nlines; i++) {
        if (i > 0) tts_buf_putc(&joined, '\n');
        tts_buf_put(&joined, lines[i], strlen(lines[i]));
        free(lines[i]);
    }
    free(lines);

    char *cur = NULL, *nxt = NULL;
    size_t cl = 0, nl = 0;
    tts_pcre2_sub("\\n{3,}", "\n\n", joined.buf, joined.len, 0, &cur, &cl);
    free(joined.buf);
    tts_pcre2_sub("[ \\t]{2,}", " ", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\s+([,.;:!?])", "\\1", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("([,.;:!?])([A-Za-z])", "\\1 \\2", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;
    tts_pcre2_sub("\\.{4,}", "...", cur, cl, 0, &nxt, &nl); free(cur); cur = nxt; cl = nl;

    /* return text.strip() — Python whitespace semantics. */
    size_t ost, oen;
    tts_py_strip_span(cur, cl, &ost, &oen);
    size_t rn = oen - ost;
    char *res = (char *)malloc(rn + 1);
    if (!res) abort();
    memcpy(res, cur + ost, rn);
    res[rn] = '\0';
    free(cur);

    free(pending_heading);
    free(lns);
    if (out_len) *out_len = rn;
    return res;
}

/* ------------------------------------------------------------------ */
/* prepare_spoken_text                                                 */
/* ------------------------------------------------------------------ */

/* Count Unicode code points (Python len(str)). */
static size_t tts_char_count(const char *s, size_t n)
{
    size_t chars = 0;
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { i += 1; }
        else if (c < 0xC0) { i += 1; }       /* stray continuation (defensive) */
        else if (c < 0xE0) i += 2;
        else if (c < 0xF0) i += 3;
        else i += 4;
        chars++;
    }
    return chars;
}

/* Python: spoken[:max_chars].rstrip() — slice by code points, never split a
 * UTF-8 sequence, then Python-rstrip trailing whitespace. */
static void tts_truncate_chars(const char *s, size_t n, int max_chars,
                               char **out, size_t *out_len)
{
    if (max_chars <= 0 || (size_t)max_chars >= tts_char_count(s, n) ||
        max_chars >= (int)n) {
        /* No truncation needed — copy + rstrip. */
        size_t ost, oen;
        tts_py_strip_span(s, n, &ost, &oen);
        size_t m = oen - ost;
        char *r = (char *)malloc(m + 1);
        if (!r) abort();
        memcpy(r, s + ost, m);
        r[m] = '\0';
        *out = r; *out_len = m;
        return;
    }
    size_t want = (size_t)max_chars;
    size_t i = 0; size_t chars = 0;
    while (i < n && chars < want) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) i += 1;
        else if (c < 0xC0) i += 1;
        else if (c < 0xE0) i += 2;
        else if (c < 0xF0) i += 3;
        else i += 4;
        chars++;
    }
    size_t end = i;
    while (end > 0) {
        char c = s[end-1];
        if (c==' '||c=='\t'||c=='\n'||c=='\r') end--; else break;
    }
    char *r = (char *)malloc(end + 1);
    if (!r) abort();
    memcpy(r, s, end);
    r[end] = '\0';
    *out = r; *out_len = end;
}

/* PoP: prepare_spoken_text @ tools/tts_text_normalize.py:prepare_spoken_text */
char *tts_prepare_spoken_text(const char *text, int max_chars, size_t *out_len)
{
    if (out_len) *out_len = 0;
    if (!text || text[0] == '\0') {
        char *e = strdup("");
        if (out_len) *out_len = 0;
        return e;
    }

    size_t l1, l2, l3, l4, l5;
    char *s1 = tts_strip_nonspoken_blocks(text, &l1);
    char *s2 = tts_strip_markdown(s1, &l2); free(s1);
    char *s3 = tts_normalize_symbols_len(s2, l2, &l3); free(s2);
    char *s4 = tts_smooth_whitespace_len(s3, l3, &l4); free(s3);
    char *s5 = tts_flatten_newlines(s4, &l5); free(s4);

    char *spoken = NULL; size_t slen = 0;
    tts_truncate_chars(s5, l5, max_chars, &spoken, &slen);
    free(s5);
    if (out_len) *out_len = slen;
    return spoken;
}
