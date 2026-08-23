/* gate: signal_format_markdown_to_signal — traced copy of
 * src/hermes_cli/signal_format.c::gateway_signal_markdown_to_signal
 * (+ normalize_bullet_markers / utf16_len), the REAL C port of
 * gateway/platforms/signal_format.py::markdown_to_signal.
 * PCRE2 is used because Python `re` is PCRE-compatible.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   Phase 1 collapse: LOOP chars L tick, B1 char=='\n' (run collapse)
 *   Phase 2 bullets:  LOOP chars L tick, B2 fence-enter, B3 in-code copy,
 *                     B4 line-start bullet rewritten
 *   Phase 3 code blocks: LOOP scan positions L tick, B5 ``` opener,
 *                     B6 closer found
 *   Phase 4 headings: LOOP matches L tick, B7 eol found vs end-of-text
 *   Phase 5 inline:   LOOP 6 patterns L tick; LOOP matches L tick,
 *                     B8 overlap-skip
 *   Phase 6 removals: LOOP matches L tick, B9 g1s>ms, B10 me>g1e;
 *                     LOOP prior styles adjust L tick
 *   Phase 7 rebuild:  LOOP matches L tick
 *   Phase 8 emit:     LOOP sorted styles L tick, B11 dropped (out-of-range/empty)
 *
 * KNOWN DIVERGENCES (documented; inputs stay in the agreed region):
 *  - C compiles ALL inline patterns with DOTALL; Python only the ** pattern.
 *    Multi-line inline content diverges -> avoided.
 *  - bullet rewrite: C requires exactly one ' ' after the marker; Py \s+
 *    eats any run. Multi-space bullets diverge -> avoided.
 *  - prior-style adjustment: Py _adjust shifts partially (min), C subtracts
 *    the full removal length when it starts before the span. A markdown
 *    marker straddling a code-block/heading span diverges -> avoided.
 *  - zero-length styles: C drops them, Py would emit "x:0:STYLE" -> avoided.
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre2.h>
#include "watercup.h"

static pcre2_code *compile_re(const char *pat) {
    int err; PCRE2_SIZE erroff;
    return pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
                         PCRE2_DOTALL, &err, &erroff, NULL);
}

/* pure predicates over buf (no trace events) */
static int at_fence(const char *s, size_t n, size_t i) {
    return s[i] == '`' && i + 2 < n + 1 && s[i+1] == '`' && s[i+2] == '`';
}
static int bullet_at(const char *s, size_t n, size_t i) {
    size_t j = (i == 0) ? 0 : i + 1;
    int indent = 0;
    while (j < n && (s[j]==' '||s[j]=='\t') && indent < 3) { j++; indent++; }
    return j < n && (s[j]=='-'||s[j]=='*'||s[j]=='+') &&
           j + 1 < n && s[j+1] == ' ';
}

typedef struct { int start; int len; char style[16]; } style_entry_t;

static void wc_markdown_to_signal(const char *input) {
    WCUP_BEGIN("markdown_to_signal");
    if (!input) input = "";
    size_t len = strlen(input);

    /* Phase 1: collapse 3+ newlines -> 2, then strip. */
    char *buf = strdup(input);
    {
        char *tmp = malloc(len + 1);
        size_t ti = 0, i = 0;
        while (i < len) {
            WCUP_LOOP_TICK();
            if (buf[i] == '\n') {                                    /* B1 */
                WCUP_BRANCH(1);
                size_t k = i; int nc = 0;
                while (k < len && buf[k] == '\n') { nc++; k++; }
                int reps = nc >= 3 ? 2 : nc;
                for (int r = 0; r < reps; r++) tmp[ti++] = '\n';
                i = k;
            } else { WCUP_BRANCH(0); tmp[ti++] = buf[i]; i++; }
        }
        tmp[ti] = '\0';
        free(buf); buf = tmp;
    }
    {
        size_t s = 0, e = strlen(buf);
        while (s < e && (buf[s]==' '||buf[s]=='\t'||buf[s]=='\n'||buf[s]=='\r')) s++;
        while (e > s && (buf[e-1]==' '||buf[e-1]=='\t'||buf[e-1]=='\n'||buf[e-1]=='\r')) e--;
        char *tmp = strdup(buf + s);
        tmp[e - s] = '\0';
        free(buf); buf = tmp;
    }

    /* Phase 2: bullet normalization. */
    {
        size_t srclen = strlen(buf);
        char *result = malloc(srclen * 3 + 4);
        size_t ri = 0, i = 0;
        int in_code = 0;
        while (i < srclen) {
            WCUP_LOOP_TICK();
            if (!in_code && at_fence(buf, srclen, i)) {              /* B2 */
                WCUP_BRANCH(1);
                result[ri++]='`'; result[ri++]='`'; result[ri++]='`';
                i += 3; in_code = 1; continue;
            }
            WCUP_BRANCH(0);
            if (in_code) {                                            /* B3 */
                WCUP_BRANCH(1);
                result[ri++] = buf[i];
                if (buf[i] == '`' && i + 2 < srclen + 1 &&
                    buf[i+1] == '`' && buf[i+2] == '`') {
                    result[ri++]='`'; result[ri++]='`';
                    i += 3; in_code = 0; continue;
                }
                i++; continue;
            }
            WCUP_BRANCH(0);
            if ((buf[i] == '\n' || i == 0) && bullet_at(buf, srclen, i)) { /* B4 */
                WCUP_BRANCH(1);
                size_t j = (i == 0) ? 0 : i + 1;
                int indent = 0;
                while (j < srclen && (buf[j]==' '||buf[j]=='\t') && indent < 3) { j++; indent++; }
                if (i != 0) result[ri++] = '\n';
                for (int k = 0; k < indent; k++) result[ri++] = ' ';
                result[ri++]='\xe2'; result[ri++]='\x80'; result[ri++]='\xa2';
                result[ri++] = ' ';
                i = j + 2; continue;
            }
            WCUP_BRANCH(0);
            result[ri++] = buf[i];
            i++;
        }
        result[ri] = '\0';
        free(buf); buf = result;
    }

    style_entry_t styles_arr[64];
    int styles_n = 0;

    /* Phase 3: code blocks ```...``` -> inner (MONOSPACE). */
    {
        typedef struct { int s, e, g1s, g1e; } cb_t;
        cb_t cbs[32]; int ncb = 0;
        size_t blen = strlen(buf);
        size_t i = 0;
        while (i + 2 < blen) {
            WCUP_LOOP_TICK();
            if (buf[i] == '`' && buf[i+1] == '`' && buf[i+2] == '`') {   /* B5 */
                WCUP_BRANCH(1);
                size_t j = i + 3;
                while (j < blen && ((buf[j]>='a'&&buf[j]<='z')||(buf[j]>='A'&&buf[j]<='Z')||
                       (buf[j]>='0'&&buf[j]<='9')||buf[j]=='_'||buf[j]=='+'||buf[j]=='-')) j++;
                if (j < blen && buf[j] == '\n') j++;
                int g1s = (int)j;
                size_t k = j;
                int found = 0;
                while (k + 2 < blen) {
                    if (buf[k]=='`'&&buf[k+1]=='`'&&buf[k+2]=='`') { found = 1; break; }
                    k++;
                }
                if (!found) break;                                       /* B6 */
                WCUP_BRANCH(1);
                int g1e = (int)k;
                while (g1e > g1s && buf[g1e-1] == '\n') g1e--;
                cbs[ncb].s=(int)i; cbs[ncb].e=(int)(k+3); cbs[ncb].g1s=g1s; cbs[ncb].g1e=g1e;
                ncb++;
                i = k + 3;
                continue;
            }
            WCUP_BRANCH(0);
            i++;
        }
        for (int kk = ncb - 1; kk >= 0; kk--) {
            int start = cbs[kk].s;
            char inner[4096];
            int ilen = cbs[kk].g1e - cbs[kk].g1s;
            memcpy(inner, buf + cbs[kk].g1s, ilen); inner[ilen] = '\0';
            styles_arr[styles_n].start = start;
            styles_arr[styles_n].len = ilen;
            strcpy(styles_arr[styles_n].style, "MONOSPACE");
            styles_n++;
            char nbuf[16384];
            memcpy(nbuf, buf, start);
            memcpy(nbuf + start, inner, ilen);
            strcpy(nbuf + start + ilen, buf + cbs[kk].e);
            free(buf); buf = strdup(nbuf);
        }
    }

    /* Phase 4: headings ^#{1,6}\s+ -> BOLD (strip prefix). */
    {
        pcre2_code *re = compile_re("^#{1,6}\\s+");
        pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
        char new_text[16384];
        size_t nti = 0, last_end = 0, blen = strlen(buf);
        PCRE2_SIZE pos = 0;
        while (1) {
            int rc = pcre2_match(re, (PCRE2_SPTR)buf, blen, pos, 0, md, NULL);
            if (rc < 0) break;
            WCUP_LOOP_TICK();
            PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
            int hs = (int)ov[0], he = (int)ov[1];
            memcpy(new_text + nti, buf + last_end, hs - last_end);
            nti += (size_t)(hs - last_end);
            int eol = (int)blen;
            for (size_t k = he; k < blen; k++) if (buf[k] == '\n') { eol = (int)k; break; }
            if (eol != (int)blen) WCUP_BRANCH(1);                       /* B7 */
            else WCUP_BRANCH(0);
            size_t htext_len = (size_t)(eol - he);
            int start = (int)nti;
            memcpy(new_text + nti, buf + he, htext_len);
            nti += htext_len;
            styles_arr[styles_n].start = start;
            styles_arr[styles_n].len = (int)htext_len;
            strcpy(styles_arr[styles_n].style, "BOLD");
            styles_n++;
            last_end = (size_t)eol;
            pos = ov[1];
            if (pos > blen) break;
        }
        memcpy(new_text + nti, buf + last_end, blen - last_end + 1);
        pcre2_match_data_free(md); pcre2_code_free(re);
        free(buf); buf = strdup(new_text);
    }

    /* Phase 5: inline patterns with overlap avoidance. */
    const char *pats[6] = {
        "\\*\\*(.+?)\\*\\*", "__(.+?)__", "~~(.+?)~~", "`(.+?)`",
        "(?<!\\*)\\*(?!\\*| )(.+?)(?<!\\*)\\*(?!\\*)", "(?<!\\w)_(?!_)(.+?)(?<!_)_(?!\\w)"
    };
    const char *sty[6] = { "BOLD","BOLD","STRIKETHROUGH","MONOSPACE","ITALIC","ITALIC" };
    typedef struct { int ms, me, g1s, g1e, pi; } im_t;
    im_t all[64]; int na = 0;
    typedef struct { int start, end; } span_t;
    span_t occ[64]; int no = 0;
    for (int p = 0; p < 6; p++) {
        WCUP_LOOP_TICK();
        pcre2_code *re = compile_re(pats[p]);
        pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
        size_t blen = strlen(buf);
        PCRE2_SIZE pos = 0;
        while (pcre2_match(re, (PCRE2_SPTR)buf, blen, pos, 0, md, NULL) >= 0) {
            WCUP_LOOP_TICK();
            PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
            int ms = (int)ov[0], me = (int)ov[1];
            int g1s = ov[2] != PCRE2_UNSET ? (int)ov[2] : ms;
            int g1e = ov[3] != PCRE2_UNSET ? (int)ov[3] : me;
            int overlaps = 0;
            for (int o = 0; o < no; o++)
                if (ms < occ[o].end && me > occ[o].start) { overlaps = 1; break; }
            if (!overlaps) {                                            /* B8 */
                WCUP_BRANCH(0);
                all[na].ms=ms; all[na].me=me; all[na].g1s=g1s; all[na].g1e=g1e; all[na].pi=p;
                na++;
                occ[no].start=ms; occ[no].end=me; no++;
            } else {
                WCUP_BRANCH(1);
            }
            if (ov[1]==ov[0]) pos=ov[1]+1; else pos=ov[1];
            if (pos>blen) break;
        }
        pcre2_match_data_free(md); pcre2_code_free(re);
    }
    for (int a = 0; a < na; a++)
        for (int b = a+1; b < na; b++)
            if (all[b].ms < all[a].ms) { im_t t=all[a]; all[a]=all[b]; all[b]=t; }

    /* Phase 6: removals + adjust prior styles. */
    typedef struct { int pos, len; } rem_t;
    rem_t rems[128]; int nr = 0;
    for (int a = 0; a < na; a++) {
        WCUP_LOOP_TICK();
        if (all[a].g1s > all[a].ms) {                                   /* B9 */
            WCUP_BRANCH(1);
            rems[nr].pos=all[a].ms; rems[nr].len=all[a].g1s-all[a].ms; nr++;
        } else WCUP_BRANCH(0);
        if (all[a].me > all[a].g1e) {                                   /* B10 */
            WCUP_BRANCH(1);
            rems[nr].pos=all[a].g1e; rems[nr].len=all[a].me-all[a].g1e; nr++;
        } else WCUP_BRANCH(0);
    }
    for (int a = 0; a < nr; a++)
        for (int b = a+1; b < nr; b++)
            if (rems[b].pos < rems[a].pos) { rem_t t=rems[a]; rems[a]=rems[b]; rems[b]=t; }
    for (int s = 0; s < styles_n; s++) {
        WCUP_LOOP_TICK();
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
    char result[16384];
    size_t ri = 0, last_end = 0;
    {
        size_t blen = strlen(buf);
        for (int a = 0; a < na; a++) {
            WCUP_LOOP_TICK();
            memcpy(result + ri, buf + last_end, (size_t)(all[a].ms - (int)last_end));
            ri += (size_t)(all[a].ms - (int)last_end);
            size_t ilen = (size_t)(all[a].g1e - all[a].g1s);
            int pos = (int)ri;
            memcpy(result + ri, buf + all[a].g1s, ilen);
            ri += ilen;
            styles_arr[styles_n].start = pos;
            styles_arr[styles_n].len = (int)ilen;
            strcpy(styles_arr[styles_n].style, sty[all[a].pi]);
            styles_n++;
            last_end = (size_t)all[a].me;
        }
        memcpy(result + ri, buf + last_end, blen - last_end + 1);
    }

    /* Phase 8: merge prior + inline, sort, drop out-of-range, UTF-16 map. */
    for (int a = 0; a < styles_n; a++)
        for (int b = a+1; b < styles_n; b++)
            if (styles_arr[b].start < styles_arr[a].start) {
                style_entry_t t=styles_arr[a]; styles_arr[a]=styles_arr[b]; styles_arr[b]=t;
            }

    size_t rlen = strlen(result);
    char out_styles[64][64]; int nout = 0;
    for (int s = 0; s < styles_n; s++) {
        WCUP_LOOP_TICK();
        if (styles_arr[s].start < 0 || styles_arr[s].start + styles_arr[s].len > (int)rlen ||
            styles_arr[s].len <= 0) {                                    /* B11 */
            WCUP_BRANCH(1);
            continue;
        }
        WCUP_BRANCH(0);
        snprintf(out_styles[nout], sizeof out_styles[nout], "%d:%d:%s",
                 styles_arr[s].start, styles_arr[s].len, styles_arr[s].style);
        nout++;
    }
    (void)rlen;
    (void)out_styles;   /* result not printed — one cup line per input only */

    free(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("markdown_to_signal");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s markdown_to_signal <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        wc_markdown_to_signal(argv[i]);
    }
    return 0;
}
