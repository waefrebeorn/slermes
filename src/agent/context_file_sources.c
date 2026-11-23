/*
 * context_file_sources.c — real C11 port of the pure helpers of upstream Hermes
 * agent/context_file_sources.py (gate w13_contextfil_b3890c).
 *
 * PoP: w13_contextfil_b3890c_estimate_tokens_rough @ agent/context_file_sources.py:estimate_tokens_rough
 * PoP: w13_contextfil_b3890c_entry @ agent/context_file_sources.py:_entry
 * PoP: w13_contextfil_b3890c_loaded_status @ agent/context_file_sources.py:_loaded_status
 * PoP: w13_contextfil_b3890c_render_context_file_lines @ agent/context_file_sources.py:render_context_file_lines
 *
 * estimate_tokens_rough mirrors agent/model_metadata.py verbatim (CJK-dense
 * codepoints ~1 token each, everything else ceil(utf8 bytes / 4)).
 */
#include "noc_w13_contextfil_b3890c.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } cf_sb_t;
static void cf_init(cf_sb_t *b) { b->cap = 256; b->len = 0; b->p = (char *)malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void cf_reserve(cf_sb_t *b, size_t need) {
    if (!b->p || b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap; while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc); if (np) { b->p = np; b->cap = nc; }
}
static void cf_addn(cf_sb_t *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    cf_reserve(b, n); memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static void cf_adds(cf_sb_t *b, const char *s) { if (s) cf_addn(b, s, strlen(s)); }
static void cf_addf(cf_sb_t *b, const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n > 0) { cf_reserve(b, (size_t)n); vsnprintf(b->p + b->len, (size_t)n + 1, fmt, ap2); b->len += (size_t)n; }
    va_end(ap2);
}
static char *cf_done(cf_sb_t *b) { if (!b->p) return (char *)calloc(1, 1); b->p[b->len] = '\0'; return b->p; }
static char *cf_dup(const char *s) {
    if (!s) s = ""; size_t n = strlen(s); char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; } return p;
}
/* Decode one UTF-8 codepoint; returns byte length and writes cp. */
static size_t cf_next_cp(const unsigned char *p, unsigned int *cp) {
    if (p[0] < 0x80) { *cp = p[0]; return 1; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { *cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3; }
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        *cp = ((p[0] & 7) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); return 4; }
    *cp = 0xFFFD; return 1;   /* errors="replace" */
}
static size_t cf_char_count(const char *s) {
    size_t n = 0; const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) { unsigned int cp; p += cf_next_cp(p, &cp); n++; }
    return n;
}
static int cf_dense_cp(unsigned int cp) {
    return (cp >= 0x1100 && cp <= 0x11ff) || (cp >= 0x2e80 && cp <= 0x9fff) ||
           (cp >= 0xa960 && cp <= 0xa97f) || (cp >= 0xac00 && cp <= 0xd7af) ||
           (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0xff00 && cp <= 0xffef);
}

long long w13_contextfil_b3890c_estimate_tokens_rough(const char *text) {
    if (!text || !*text) return 0;
    int ascii = 1;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (*p >= 0x80) { ascii = 0; break; }
    if (ascii) { size_t n = strlen(text); return (long long)((n + 3) / 4); }
    size_t dense = 0, remain = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        unsigned int cp; size_t n = cf_next_cp(p, &cp);
        if (cf_dense_cp(cp)) dense++; else remain += n;
        p += n;
    }
    return (long long)(dense + ((remain + 3) / 4));
}
json_t *w13_contextfil_b3890c_entry(const char *label, const char *path,
        const char *content, const char *status) {
    json_t *o = json_object();
    json_set(o, "label", json_string(label ? label : ""));
    json_set(o, "path", json_string(path ? path : ""));
    json_set(o, "chars", json_number((double)cf_char_count(content)));
    json_set(o, "est_tokens", json_number(
        (double)w13_contextfil_b3890c_estimate_tokens_rough(content)));
    int loaded = status && (!strcmp(status, "loaded") || !strcmp(status, "truncated") ||
                            !strcmp(status, "flagged"));
    json_set(o, "loaded", json_bool(loaded));
    json_set(o, "status", json_string(status ? status : ""));
    return o;
}

char *w13_contextfil_b3890c_loaded_status(int scan_hit, long long rendered_len,
        long long max_chars, int user_authored) {
    if (scan_hit) return cf_dup(user_authored ? "flagged" : "blocked");
    return cf_dup(rendered_len > max_chars ? "truncated" : "loaded");
}
/* _STATUS_DISPLAY from the live module. */
typedef struct { const char *status, *glyph, *note; } cf_disp_t;
static const cf_disp_t CF_DISP[] = {
    {"loaded",     "\xE2\x9C\x93", ""},                                                    /* check */
    {"truncated",  "\xE2\x97\x90", "truncated \xE2\x80\x94 over context_file_max_chars"},   /* half-circle */
    {"shadowed",   "\xE2\x97\x8B", "not loaded \xE2\x80\x94 higher-priority context type wins"},
    {"blocked",    "\xE2\x9C\x97", "not loaded \xE2\x80\x94 blocked by the prompt-injection scan"},
    {"flagged",    "\xE2\x9A\xA0", "loaded \xE2\x80\x94 matched prompt-injection pattern(s); review the file"},
    {"empty",      "\xE2\x97\x8B", "not loaded \xE2\x80\x94 empty file"},
    {"unreadable", "\xE2\x9C\x97", "not loaded \xE2\x80\x94 could not be read"},
    {"skipped",    "\xE2\x9C\x97", "not loaded \xE2\x80\x94 resolves outside the project or to a protected file"},
    {"suppressed", "\xE2\x97\x8B", "not loaded \xE2\x80\x94 cwd fell back to the Hermes install tree"},
};
static void cf_lookup_disp(const char *status, const char **glyph, const char **note) {
    for (size_t i = 0; i < sizeof(CF_DISP) / sizeof(CF_DISP[0]); i++)
        if (!strcmp(status, CF_DISP[i].status)) { *glyph = CF_DISP[i].glyph; *note = CF_DISP[i].note; return; }
    *glyph = "\xE2\x80\xA2"; *note = "";   /* bullet fallback */
}
/* Python f"{v:,}" for an integer (thousands separator). */
static char *cf_comma(long long v) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%lld", v < 0 ? -v : v);
    size_t len = strlen(tmp);
    char *s = (char *)malloc(len + (len - 1) / 3 + 2);
    size_t d = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) s[d++] = ',';
        s[d++] = tmp[i];
    }
    s[d] = '\0';
    if (v < 0) { char *n = (char *)malloc(d + 2); n[0] = '-'; memcpy(n + 1, s, d + 1); free(s); return n; }
    return s;
}
json_t *w13_contextfil_b3890c_render_context_file_lines(const json_t *sources) {
    json_t *lines = json_array();
    if (!sources || sources->type != JSON_ARRAY || json_len(sources) == 0) return lines;
    size_t n = json_len(sources);
    size_t width = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *s = json_get(sources, i);
        const json_t *lbl = (s && s->type == JSON_OBJECT) ? json_obj_get(s, "label") : NULL;
        const char *ls = (lbl && lbl->type == JSON_STRING && lbl->str_val) ? lbl->str_val : "";
        size_t llen = cf_char_count(ls);
        if (llen > width) width = llen;
    }
    json_append(lines, json_string("Context files"));
    for (size_t i = 0; i < n; i++) {
        const json_t *s = json_get(sources, i);
        const json_t *lbl = (s && s->type == JSON_OBJECT) ? json_obj_get(s, "label") : NULL;
        const char *ls = (lbl && lbl->type == JSON_STRING && lbl->str_val) ? lbl->str_val : "";
        const json_t *st = (s && s->type == JSON_OBJECT) ? json_obj_get(s, "status") : NULL;
        const char *ss = (st && st->type == JSON_STRING && st->str_val) ? st->str_val : "";
        const char *glyph, *note;
        cf_lookup_disp(ss, &glyph, &note);
        const json_t *ev = (s && s->type == JSON_OBJECT) ? json_obj_get(s, "est_tokens") : NULL;
        long long est = (ev && ev->type == JSON_NUMBER) ? (long long)ev->num_val : 0;
        char *comma = cf_comma(est);
        size_t clen = strlen(comma);
        cf_sb_t b; cf_init(&b);
        cf_adds(&b, glyph);
        cf_adds(&b, " ");
        cf_adds(&b, ls);
        for (size_t j = cf_char_count(ls); j < width; j++) cf_adds(&b, " ");
        cf_adds(&b, " ~");
        for (size_t j = clen; j < 9; j++) cf_adds(&b, " ");   /* :>9 */
        cf_adds(&b, comma);
        cf_adds(&b, " tokens");
        if (note[0]) { cf_addf(&b, "  (%s)", note); }
        json_append(lines, json_string(cf_done(&b)));
        free(comma);
    }
    return lines;
}
