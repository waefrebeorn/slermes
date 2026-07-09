/*
 * port_gateway_signal_format.c — C port of gateway/platforms/signal_format.py
 *
 * markdown_to_signal(): convert markdown to plain text + Signal textStyles list
 * (format "start:length:STYLE", positions in UTF-16 code units). Pure stdlib +
 * PCRE2 (Python's `re` is PCRE-compatible, so PCRE2 reproduces it faithfully).
 *
 * Faithful to the LIVE Python: regex passes, code-block stripping, heading
 * extraction, inline-style overlap avoidance, marker removal, and UTF-16
 * position mapping. Verified byte-equal to LIVE Python via
 * tests/sta_oracle_signal_format.py.
 */

#define PCRE2_CODE_UNIT_WIDTH 8

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2.h>

/* ---------- small helpers ---------- */

static char *xstrdup(const char *s) { return strdup(s ? s : ""); }

/* UTF-16 code-unit length of the first n bytes of s (matches Python
 * len(s.encode("utf-16-le"))//2 for the prefix). */
static size_t utf16_len(const char *s, size_t n)
{
    size_t u16 = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c & 0x80) == 0) u16 += 1;
        else if ((c & 0xE0) == 0xC0) { u16 += 1; i += 1; }
        else if ((c & 0xF0) == 0xE0) { u16 += 1; i += 2; }
        else if ((c & 0xF8) == 0xF0) { u16 += 2; i += 3; }
        else u16 += 1;
    }
    return u16;
}

/* PCRE2 compile with error reporting; never returns NULL unchecked. */
static pcre2_code *compile_re_flags(const char *pat, uint32_t extra)
{
    int err; PCRE2_SIZE erroff;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
                                   PCRE2_DOTALL | extra, &err, &erroff, NULL);
    if (!re) {
        char buf[256];
        pcre2_get_error_message(err, buf, sizeof(buf));
        fprintf(stderr, "signal_format: pcre2_compile failed at %zu: %s [%s]\n",
                erroff, buf, pat);
        abort();
    }
    return re;
}
static pcre2_code *compile_re(const char *pat) { return compile_re_flags(pat, 0); }

/* ---------- bullet normalization (preserve fenced code) ---------- */

static char *normalize_bullet_markers(const char *src)
{
    size_t srclen = strlen(src);
    char *result = malloc(srclen * 2 + 1);
    size_t ri = 0;
    size_t i = 0;
    int in_code = 0;
    while (i < srclen) {
        /* enter a fenced code block */
        if (!in_code && i + 2 < srclen + 1 && src[i] == '`' && src[i+1] == '`' && src[i+2] == '`') {
            result[ri++] = '`'; result[ri++] = '`'; result[ri++] = '`';
            i += 3; in_code = 1; continue;
        }
        if (in_code) {
            result[ri++] = src[i];
            if (src[i] == '`' && i + 2 < srclen + 1 && src[i+1] == '`' && src[i+2] == '`') {
                result[ri++] = '`'; result[ri++] = '`';
                i += 3; in_code = 0; continue;
            }
            i++; continue;
        }
        /* prose: leading bullet marker at line start -> "• " */
        if (src[i] == '\n' || i == 0) {
            size_t j = (i == 0) ? 0 : i + 1;
            int indent = 0;
            while (j < srclen && (src[j] == ' ' || src[j] == '\t') && indent < 3) { j++; indent++; }
            if (j < srclen && (src[j] == '-' || src[j] == '*' || src[j] == '+')
                && j + 1 < srclen && src[j+1] == ' ') {
                if (i != 0) result[ri++] = '\n';
                for (int k = 0; k < indent; k++) result[ri++] = ' ';
                result[ri++] = '\xe2'; result[ri++] = '\x80'; result[ri++] = '\xa2'; /* • */
                result[ri++] = ' ';
                i = j + 2; continue;
            }
        }
        result[ri++] = src[i];
        i++;
    }
    result[ri] = '\0';
    return result;
}

/* ---------- main entry ---------- */

typedef struct { int start; int len; char style[16]; } style_entry_t;

/* PoP: gateway_signal_markdown_to_signal @ gateway/platforms/signal_format.py:markdown_to_signal */

/* Convert markdown -> (text, styles[]). Caller owns *out_text (malloc) and each
 * style string in styles[] (malloc). Returns number of style strings written. */
int gateway_signal_markdown_to_signal(const char *input,
                                       char **out_text,
                                       char *styles[], int styles_cap, int *out_n)
{
    if (!input) input = "";
    size_t len = strlen(input);

    /* Phase 1: collapse 3+ newlines -> 2, then strip. */
    char *buf = xstrdup(input);
    {
        char *tmp = malloc(len + 1);
        size_t ti = 0, i = 0;
        while (i < len) {
            if (buf[i] == '\n') {
                size_t k = i; int nc = 0;
                while (k < len && buf[k] == '\n') { nc++; k++; }
                int reps = nc >= 3 ? 2 : nc;
                for (int r = 0; r < reps; r++) tmp[ti++] = '\n';
                i = k;
            } else { tmp[ti++] = buf[i]; i++; }
        }
        tmp[ti] = '\0';
        free(buf); buf = tmp;
    }
    {
        size_t s = 0, e = strlen(buf);
        while (s < e && (buf[s]==' '||buf[s]=='\t'||buf[s]=='\n'||buf[s]=='\r')) s++;
        while (e > s && (buf[e-1]==' '||buf[e-1]=='\t'||buf[e-1]=='\n'||buf[e-1]=='\r')) e--;
        char *tmp = xstrdup(buf + s);
        tmp[e - s] = '\0';
        free(buf); buf = tmp;
    }

    /* Phase 2: bullet normalization. */
    char *bulleted = normalize_bullet_markers(buf);
    free(buf); buf = bulleted;

    style_entry_t *styles_arr = NULL;
    int styles_n = 0, styles_cap_dyn = 0;
    #define PUSH_STYLE(st, ln, sty) do { \
        if (styles_n >= styles_cap_dyn) { styles_cap_dyn = styles_cap_dyn?styles_cap_dyn*2:16; \
            styles_arr = realloc(styles_arr, styles_cap_dyn*sizeof(style_entry_t)); } \
        styles_arr[styles_n].start = (st); styles_arr[styles_n].len = (ln); \
        strncpy(styles_arr[styles_n].style, (sty), 15); styles_arr[styles_n].style[15]='\0'; \
        styles_n++; } while(0)

    /* Phase 3: code blocks ```...``` -> inner (MONOSPACE).
     * Manual scan (faithful to Python regex ```[a-zA-Z0-9_+-]*\n?(.*?)```):
     * drop the opening fence + optional language tag + optional newline, keep
     * the inner content up to the closing fence, rstrip trailing newlines. */
    {
        typedef struct { int s, e, g1s, g1e; } cb_t;
        cb_t *cbs = NULL; int ncb = 0, ccap = 0;
        size_t blen = strlen(buf);
        size_t i = 0;
        while (i + 2 < blen) {
            if (buf[i] == '`' && buf[i+1] == '`' && buf[i+2] == '`') {
                size_t j = i + 3;
                /* skip language tag */
                while (j < blen && ((buf[j]>='a'&&buf[j]<='z')||(buf[j]>='A'&&buf[j]<='Z')||
                       (buf[j]>='0'&&buf[j]<='9')||buf[j]=='_'||buf[j]=='+'||buf[j]=='-')) j++;
                /* optional single newline */
                if (j < blen && buf[j] == '\n') j++;
                int g1s = (int)j;
                /* find closing fence */
                size_t k = j;
                int found = 0;
                while (k + 2 < blen) {
                    if (buf[k]=='`'&&buf[k+1]=='`'&&buf[k+2]=='`') { found = 1; break; }
                    k++;
                }
                if (!found) break;
                int g1e = (int)k;
                /* rstrip trailing newlines from inner */
                while (g1e > g1s && buf[g1e-1] == '\n') g1e--;
                if (ncb+1>=ccap){ccap=ccap?ccap*2:8;cbs=realloc(cbs,ccap*sizeof(cb_t));}
                cbs[ncb].s=(int)i; cbs[ncb].e=(int)(k+3); cbs[ncb].g1s=g1s; cbs[ncb].g1e=g1e;
                ncb++;
                i = k + 3;
                continue;
            }
            i++;
        }
        char *nbuf = xstrdup(buf);
        for (int kk = ncb - 1; kk >= 0; kk--) {
            int start = cbs[kk].s;
            char *inner = xstrdup(buf + cbs[kk].g1s);
            inner[cbs[kk].g1e - cbs[kk].g1s] = '\0';
            PUSH_STYLE(start, (int)strlen(inner), "MONOSPACE");
            size_t newlen = (size_t)start + strlen(inner) + (strlen(nbuf) - cbs[kk].e) + 1;
            char *repl = malloc(newlen);
            memcpy(repl, nbuf, start);
            memcpy(repl + start, inner, strlen(inner));
            memcpy(repl + start + strlen(inner), nbuf + cbs[kk].e, strlen(nbuf) - cbs[kk].e + 1);
            free(inner); free(nbuf); nbuf = repl;
        }
        free(cbs);
        free(buf); buf = nbuf;
    }

    /* Phase 4: headings ^#{1,6}\s+ -> BOLD (strip prefix). */
    {
        pcre2_code *re = compile_re_flags("^#{1,6}\\s+", PCRE2_MULTILINE);
        pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
        char *new_text = malloc(strlen(buf) + 1);
        size_t nti = 0, last_end = 0, blen = strlen(buf);
        PCRE2_SIZE pos = 0;
        while (1) {
            int rc = pcre2_match(re, (PCRE2_SPTR)buf, blen, pos, 0, md, NULL);
            if (rc < 0) break;
            PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
            int hs = (int)ov[0], he = (int)ov[1];
            memcpy(new_text + nti, buf + last_end, hs - last_end);
            nti += (hs - last_end);
            int eol = (int)blen;
            for (size_t k = he; k < blen; k++) if (buf[k] == '\n') { eol = (int)k; break; }
            size_t htext_len = (size_t)(eol - he);
            int start = (int)nti;
            memcpy(new_text + nti, buf + he, htext_len);
            nti += htext_len;
            PUSH_STYLE(start, (int)htext_len, "BOLD");
            last_end = (size_t)eol;
            pos = ov[1];
            if (pos > blen) break;
        }
        memcpy(new_text + nti, buf + last_end, blen - last_end + 1);
        pcre2_match_data_free(md); pcre2_code_free(re);
        free(buf); buf = new_text;
    }

    /* Phase 5: inline patterns with overlap avoidance. */
    const char *pats[6] = {
        "\\*\\*(.+?)\\*\\*", "__(.+?)__", "~~(.+?)~~", "`(.+?)`",
        "(?<!\\*)\\*(?!\\*| )(.+?)(?<!\\*)\\*(?!\\*)", "(?<!\\w)_(?!_)(.+?)(?<!_)_(?!\\w)"
    };
    const char *sty[6] = { "BOLD","BOLD","STRIKETHROUGH","MONOSPACE","ITALIC","ITALIC" };
    typedef struct { int ms, me, g1s, g1e; int pi; } im_t;
    im_t *all = NULL; int na = 0, acap = 0;
    typedef struct { int start; int end; } span_t;
    span_t *occ = NULL; int no = 0, ocap = 0;
    for (int p = 0; p < 6; p++) {
        pcre2_code *re = compile_re(pats[p]);
        pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
        size_t blen = strlen(buf);
        PCRE2_SIZE pos = 0;
        while (pcre2_match(re, (PCRE2_SPTR)buf, blen, pos, 0, md, NULL) >= 0) {
            PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
            int ms = (int)ov[0], me = (int)ov[1];
            int g1s = ov[2] != PCRE2_UNSET ? (int)ov[2] : ms;
            int g1e = ov[3] != PCRE2_UNSET ? (int)ov[3] : me;
            int overlaps = 0;
            for (int o = 0; o < no; o++) if (ms < occ[o].end && me > occ[o].start) { overlaps = 1; break; }
            if (!overlaps) {
                if (na+1>=acap){acap=acap?acap*2:16;all=realloc(all,acap*sizeof(im_t));}
                all[na].ms=ms;all[na].me=me;all[na].g1s=g1s;all[na].g1e=g1e;all[na].pi=p;
                na++;
                if (no+1>=ocap){ocap=ocap?ocap*2:16;occ=realloc(occ,ocap*sizeof(span_t));}
                occ[no].start=ms;occ[no].end=me;no++;
            }
            if (ov[1]==ov[0]) pos=ov[1]+1; else pos=ov[1];
            if (pos>blen) break;
        }
        pcre2_match_data_free(md); pcre2_code_free(re);
    }
    for (int a = 0; a < na; a++)
        for (int b = a+1; b < na; b++)
            if (all[b].ms < all[a].ms) { im_t t=all[a]; all[a]=all[b]; all[b]=t; }

    /* Phase 6: removals (strip markdown markers) + adjust prior styles. */
    typedef struct { int pos, len; } rem_t;
    rem_t *rems = NULL; int nr = 0, rcap = 0;
    for (int a = 0; a < na; a++) {
        if (all[a].g1s > all[a].ms) {
            if (nr+1>=rcap){rcap=rcap?rcap*2:16;rems=realloc(rems,rcap*sizeof(rem_t));}
            rems[nr].pos=all[a].ms; rems[nr].len=all[a].g1s-all[a].ms; nr++;
        }
        if (all[a].me > all[a].g1e) {
            if (nr+1>=rcap){rcap=rcap?rcap*2:16;rems=realloc(rems,rcap*sizeof(rem_t));}
            rems[nr].pos=all[a].g1e; rems[nr].len=all[a].me-all[a].g1e; nr++;
        }
    }
    for (int a = 0; a < nr; a++)
        for (int b = a+1; b < nr; b++)
            if (rems[b].pos < rems[a].pos) { rem_t t=rems[a]; rems[a]=rems[b]; rems[b]=t; }
    for (int s = 0; s < styles_n; s++) {
        int shift = 0;
        for (int r = 0; r < nr; r++) { if (rems[r].pos < styles_arr[s].start) shift += rems[r].len; else break; }
        styles_arr[s].start -= shift;
        int end = styles_arr[s].start + styles_arr[s].len;
        int end_shift = 0;
        for (int r = 0; r < nr; r++) { if (rems[r].pos < end) end_shift += rems[r].len; else break; }
        end -= end_shift;
        styles_arr[s].len = end - styles_arr[s].start;
    }

    /* Phase 7: rebuild text + inline styles. */
    char *result = malloc(strlen(buf) + 1);
    size_t ri = 0, last_end = 0;
    for (int a = 0; a < na; a++) {
        memcpy(result + ri, buf + last_end, all[a].ms - last_end);
        ri += (all[a].ms - last_end);
        size_t ilen = (size_t)(all[a].g1e - all[a].g1s);
        int pos = (int)ri;
        memcpy(result + ri, buf + all[a].g1s, ilen);
        ri += ilen;
        PUSH_STYLE(pos, (int)ilen, sty[all[a].pi]);
        last_end = all[a].me;
    }
    memcpy(result + ri, buf + last_end, strlen(buf) - last_end + 1);

    /* Phase 8: merge prior + inline, sort, drop out-of-range, UTF-16 map. */
    for (int a = 0; a < styles_n; a++)
        for (int b = a+1; b < styles_n; b++)
            if (styles_arr[b].start < styles_arr[a].start) { style_entry_t t=styles_arr[a]; styles_arr[a]=styles_arr[b]; styles_arr[b]=t; }

    char **out_styles = NULL;
    int nout = 0;
    size_t rlen = strlen(result);
    for (int s = 0; s < styles_n; s++) {
        if (styles_arr[s].start < 0 || styles_arr[s].start + styles_arr[s].len > (int)rlen) continue;
        if (styles_arr[s].len <= 0) continue;
        size_t u16_start = utf16_len(result, (size_t)styles_arr[s].start);
        size_t u16_len = utf16_len(result + styles_arr[s].start, (size_t)styles_arr[s].len);
        char ss[64];
        snprintf(ss, sizeof(ss), "%zu:%zu:%s", u16_start, u16_len, styles_arr[s].style);
        out_styles = realloc(out_styles, sizeof(char*) * (nout + 1));
        out_styles[nout++] = xstrdup(ss);
    }

    *out_text = result;
    *out_n = nout;
    int cap = nout < styles_cap ? nout : styles_cap;
    for (int i = 0; i < cap; i++) styles[i] = out_styles[i];
    for (int i = cap; i < nout; i++) free(out_styles[i]);
    free(out_styles);
    free(all); free(occ); free(rems); free(styles_arr);
    return nout;
}
