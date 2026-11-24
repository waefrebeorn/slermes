/*
 * redact_approval_display.c — real C11 port of selected behavior of upstream
 * Hermes agent/redact_approval_display.py.
 *
 * PoP: w17_redactappr_569956_sanitize @ agent/redact_approval_display.py:sanitize_command_for_display
 */
#include "noc_w17_redactappr_569956.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t *v; size_t n, cap; } Cps;

static char *dupstr(const char *s) {
    size_t n; char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static void cps_push(Cps *a, uint32_t cp) {
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 64;
        uint32_t *nv = (uint32_t *)realloc(a->v, nc * sizeof(uint32_t));
        if (!nv) return; /* OOM: drop the code point rather than crash */
        a->v = nv; a->cap = nc;
    }
    a->v[a->n++] = cp;
}

static void cps_lit(Cps *a, const char *s) {
    for (; *s; s++) cps_push(a, (unsigned char)*s);
}

static void cps_dec(Cps *a, long n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", n);
    cps_lit(a, buf);
}

/* Decode UTF-8 into code points. Invalid continuation/lead bytes are kept as
 * their raw byte value (matches Python's permissive surrogateescape path). */
static Cps decode_utf8(const char *s) {
    Cps a = {NULL, 0, 0};
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        uint32_t cp;
        unsigned char b0 = p[0];
        if (b0 < 0x80) { cp = b0; p++; }
        else if ((b0 & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((uint32_t)(b0 & 0x1F) << 6) | (p[1] & 0x3F); p += 2;
        } else if ((b0 & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3;
        } else if ((b0 & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) |
                 ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4;
        } else { cp = b0; p++; }
        cps_push(&a, cp);
    }
    return a;
}

static char *encode_utf8(const Cps *a) {
    size_t i, cap = a->n * 4 + 1;
    char *out = (char *)malloc(cap);
    size_t k = 0;
    if (!out) return NULL;
    for (i = 0; i < a->n; i++) {
        uint32_t cp = a->v[i];
        if (cp < 0x80) out[k++] = (char)cp;
        else if (cp < 0x800) {
            out[k++] = (char)(0xC0 | (cp >> 6));
            out[k++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[k++] = (char)(0xE0 | (cp >> 12));
            out[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[k++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[k++] = (char)(0xF0 | (cp >> 18));
            out[k++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[k++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    out[k] = 0;
    return out;
}

/* _APPROVAL_NON_ASCII_RE = [^\x20-\x7e\t\n]+ */
static int is_special(uint32_t cp) {
    if (cp == '\t' || cp == '\n') return 0;
    return !(cp >= 0x20 && cp <= 0x7e);
}

static int blank_glyph(uint32_t cp) {
    if (cp == 0x034F || cp == 0x115F || cp == 0x1160 || cp == 0x17B4 ||
        cp == 0x17B5 || cp == 0x180B || cp == 0x180C || cp == 0x180D ||
        cp == 0x180F || cp == 0x2800 || cp == 0x3164 || cp == 0xFFA0) return 1;
    if (cp >= 0xFE00 && cp <= 0xFE0F) return 1;
    if (cp >= 0xE0100 && cp <= 0xE01EF) return 1;
    return 0;
}

static int prose_bidi(uint32_t cp) {
    if (cp >= 0x202A && cp <= 0x202E) return 1;
    if (cp >= 0x2066 && cp <= 0x2069) return 1;
    return 0;
}

/* unicodedata.category(cp)[0] == 'C' (Cc/Cf/Cs/Co), plus Zl/Zp handled by callers. */
static int cat_is_C(uint32_t cp) {
    if (cp <= 0x1F || (cp >= 0x7F && cp <= 0x9F)) return 1;          /* Cc */
    if (cp >= 0xD800 && cp <= 0xDFFF) return 1;                      /* Cs */
    if (cp >= 0xE000 && cp <= 0xF8FF) return 1;                      /* Co (BMP private use) */
    if (cp >= 0xF0000 && cp <= 0xFFFFD) return 1;                    /* Co (plane 15) */
    if (cp >= 0x100000 && cp <= 0x10FFFD) return 1;                  /* Co (plane 16) */
    if (cp == 0x00AD || (cp >= 0x0600 && cp <= 0x0605) || cp == 0x061C ||
        cp == 0x06DD || cp == 0x070F || (cp >= 0x0890 && cp <= 0x0891) ||
        cp == 0x08E2 || cp == 0x180E ||
        (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x202A && cp <= 0x202E) ||
        (cp >= 0x2060 && cp <= 0x2064) || (cp >= 0x2066 && cp <= 0x206F) ||
        cp == 0xFEFF || (cp >= 0xFFF9 && cp <= 0xFFFB) || cp == 0x110BD ||
        cp == 0x110CD || (cp >= 0x13430 && cp <= 0x1343F) ||
        (cp >= 0x1BCA0 && cp <= 0x1BCA3) || (cp >= 0x1D173 && cp <= 0x1D17A) ||
        cp == 0xE0001 || (cp >= 0xE0020 && cp <= 0xE007F)) return 1; /* Cf */
    return 0;
}

static int is_hidden(uint32_t cp, int prose) {
    int isCc = (cp <= 0x1F) || (cp >= 0x7F && cp <= 0x9F);
    int isSep = (cp == 0x2028 || cp == 0x2029); /* Zl / Zp */
    if (prose) return isCc || isSep || prose_bidi(cp) ? 1 : 0;
    if (blank_glyph(cp)) return 1;
    return cat_is_C(cp) || isSep ? 1 : 0;
}

/* Python str.encode("unicode_escape").decode("ascii") for ONE code point. */
static void append_escape(Cps *a, uint32_t cp, int n) {
    char esc[16];
    int i;
    if (cp == 0x0D) snprintf(esc, sizeof(esc), "\\r"); /* unicode_escape names CR */
    else if (cp <= 0xFF) snprintf(esc, sizeof(esc), "\\x%02x", (unsigned)cp);
    else if (cp <= 0xFFFF) snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)cp);
    else snprintf(esc, sizeof(esc), "\\U%08x", (unsigned)cp);

    if (n < 4) {
        for (i = 0; i < n; i++) cps_lit(a, esc);
        return;
    }
    cps_push(a, 0x27E8);         /* ⟨ */
    cps_lit(a, esc);
    cps_push(a, 0x20);           /* space */
    cps_push(a, 0x00D7);         /* × */
    cps_dec(a, n);
    cps_push(a, 0x27E9);         /* ⟩ */
}

/* _APPROVAL_NON_ASCII_RE + _APPROVAL_SAME_CHAR_RUN_RE */
static Cps escape_runs(const Cps *in, int prose) {
    Cps out = {NULL, 0, 0};
    size_t i = 0;
    while (i < in->n) {
        uint32_t cp = in->v[i];
        if (is_special(cp)) {
            size_t j = i + 1, count;
            while (j < in->n && in->v[j] == cp) j++;
            count = j - i;
            if (is_hidden(cp, prose)) append_escape(&out, cp, (int)count);
            else while (count--) cps_push(&out, cp);
            i = j;
        } else {
            cps_push(&out, cp);
            i++;
        }
    }
    return out;
}

static int approval_space(uint32_t cp) {
    return cp == ' ' || cp == '\t' || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x202F ||
           cp == 0x205F || cp == 0x3000;
}

/* Python regex \s (unicode whitespace), used by the pad lookbehind \S. */
static int py_ws(uint32_t cp) {
    if (cp >= 0x09 && cp <= 0x0D) return 1;
    if (cp >= 0x1C && cp <= 0x1F) return 1;
    return approval_space(cp) || cp == 0x85 || cp == 0x2028 || cp == 0x2029;
}

/* _APPROVAL_BLANK_RE: collapse 3+ whitespace-only lines. */
static Cps collapse_blanks(const Cps *in) {
    Cps out = {NULL, 0, 0};
    size_t i = 0;
    while (i < in->n) {
        if (in->v[i] == '\n') {
            size_t j = i + 1, count = 1, reps = 0;
            for (;;) {
                size_t k = j;
                while (k < in->n && approval_space(in->v[k])) k++;
                if (k < in->n && in->v[k] == '\n') { reps++; count++; j = k + 1; }
                else break;
            }
            if (reps >= 3) {
                cps_push(&out, '\n');
                cps_push(&out, 0x27E8); /* ⟨ */
                cps_push(&out, '+');
                cps_dec(&out, (long)(count - 1));
                cps_lit(&out, " blank lines");
                cps_push(&out, 0x27E9); /* ⟩ */
                cps_push(&out, '\n');
                i = j;
                continue;
            }
        }
        cps_push(&out, in->v[i]);
        i++;
    }
    return out;
}

/* _APPROVAL_PAD_RE: collapse long whitespace runs. */
static Cps collapse_pad(const Cps *in) {
    Cps out = {NULL, 0, 0};
    size_t i = 0;
    int at_line_start = 1;
    while (i < in->n) {
        uint32_t cp = in->v[i];
        if (approval_space(cp)) {
            size_t L = 0;
            int alt1 = (i > 0 && !py_ws(in->v[i - 1]));
            int alt2 = at_line_start;
            while (i + L < in->n && approval_space(in->v[i + L])) L++;
            if ((alt1 && L >= 21) || (alt2 && L >= 65)) {
                cps_push(&out, ' ');
                cps_push(&out, 0x27E8); /* ⟨ */
                cps_push(&out, '+');
                cps_dec(&out, (long)L);
                cps_lit(&out, " whitespace chars");
                cps_push(&out, 0x27E9); /* ⟩ */
                cps_push(&out, ' ');
                i += L;
                at_line_start = 0;
                continue;
            }
        }
        cps_push(&out, cp);
        at_line_start = (cp == '\n') ? 1 : 0;
        i++;
    }
    return out;
}

char *w17_redactappr_569956_sanitize(const char *text, int prose) {
    Cps a, b, c;
    char *res;
    if (!text || !*text) return dupstr("");
    a = decode_utf8(text);
    b = escape_runs(&a, prose ? 1 : 0);
    c = collapse_blanks(&b);
    /* third pass replaces into a new buffer; free c after */
    {
        Cps d = collapse_pad(&c);
        res = encode_utf8(&d);
        free(d.v);
    }
    free(a.v); free(b.v); free(c.v);
    return res ? res : dupstr("");
}
