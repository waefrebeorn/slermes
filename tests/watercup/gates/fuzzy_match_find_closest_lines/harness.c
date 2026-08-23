/* gate: fuzzy_match_find_closest_lines — traced copy of
 * src/hermes_cli/fuzzy_match_helpers.c::fuzzy_match_find_closest_lines
 * (LIVE twin: tools/fuzzy_match.py::find_closest_lines).
 *
 * Input per argv token: JSON [old_string, content, context_lines, max_results].
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: empty old_string or content      -> ""
 *   B2: no old lines or no content lines -> ""
 *   B3: first line of old_string blank   -> fallback scan
 *     per-old-line: L tick, B4 stripped non-blank
 *   B5: still no anchor                  -> ""
 *   per-content-line scoring: L tick,
 *   B6 stripped line is blank (skip), B7 ratio > 0.3 (scored)
 *   B8: nothing scored                   -> ""
 *   per-top-candidate emit: L tick, B9 duplicate range (skip)
 *   B10: nothing emitted                 -> ""
 *   V : selected 1-based content line numbers ("i.j.k")
 *
 * NOTE: V carries the selected candidate indices rather than the formatted
 * result string because the C port appends a trailing '\n' per emitted block
 * while LIVE python does not — a real formatting divergence documented in
 * gate.json notes. The branch/selection ladder itself is faithful.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"
#include <json.h>

/* --- SequenceMatcher.ratio() via LCS (verbatim port helper) -------------- */
static double seq_ratio(const char *a, size_t alen, const char *b, size_t blen)
{
    if (alen == 0 && blen == 0) return 1.0;
    if (alen == 0 || blen == 0) return 0.0;
    if ((double)alen * (double)blen > 4000000.0) {
        size_t common = 0;
        size_t m = alen < blen ? alen : blen;
        for (size_t i = 0; i < m; i++) if (a[i] == b[i]) common++; else break;
        return (double)(2 * common) / (double)(alen + blen);
    }
    size_t *prev = malloc((blen + 1) * sizeof(size_t));
    size_t *cur = malloc((blen + 1) * sizeof(size_t));
    for (size_t j = 0; j <= blen; j++) { prev[j] = 0; cur[j] = 0; }
    for (size_t i = 1; i <= alen; i++) {
        for (size_t j = 1; j <= blen; j++) {
            if (a[i - 1] == b[j - 1]) cur[j] = prev[j - 1] + 1;
            else cur[j] = prev[j] > cur[j - 1] ? prev[j] : cur[j - 1];
        }
        for (size_t j = 0; j <= blen; j++) prev[j] = cur[j];
    }
    size_t M = prev[blen];
    free(prev); free(cur);
    return (double)(2 * M) / (double)(alen + blen);
}

static char *xstrndup(const char *s, size_t n)
{
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* split on '\n' (mirrors str.split('\n')) */
static size_t split_lines(const char *s, char ***out)
{
    size_t n = 0, cap = 0;
    char **lines = NULL;
    /* mirrors str.splitlines(): a trailing '\n' does NOT produce a final
     * empty line (unlike str.split('\n')) */
    const char *p = s;
    size_t slen = strlen(s);
    while (1) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : slen - (size_t)(p - s);
        if (n >= cap) { cap = cap ? cap * 2 : 8; lines = realloc(lines, cap * sizeof(char *)); }
        if (!(nl && nl[1] == '\0')) {
            /* normal line (or final line without newline) */
            lines[n++] = xstrndup(p, len);
        } else {
            /* line followed by trailing newline at end of string: emit it
             * only if it is non-empty OR there were earlier chars */
            lines[n++] = xstrndup(p, len);
            break;
        }
        if (!nl) break;
        p = nl + 1;
        if (*p == '\0') break;   /* trailing newline: no final empty line */
    }
    *out = lines;
    return n;
}

static void free_lines(char **lines, size_t n)
{
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

/* strip in a fresh buffer */
static char *strip_dup(const char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r'))
        len--;
    return xstrndup(s, len);
}

static void trace_find_closest(const char *old_string, const char *content,
                               int context_lines, int max_results)
{
    WCUP_BEGIN("find_closest_lines");
    char sel[256];
    sel[0] = '\0';

    int empty_in = !old_string || !*old_string || !content || !*content;
    WCUP_BRANCH(empty_in);                                     /* B1 */
    if (empty_in) {
        WCUP_VALSTR(sel);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("find_closest_lines");
        return;
    }

    char **ol = NULL, **cl = NULL;
    size_t on = split_lines(old_string, &ol);
    size_t cn = split_lines(content, &cl);

    int no_lines = on == 0 || cn == 0;
    WCUP_BRANCH(no_lines);                                     /* B2 */
    if (no_lines) {
        free_lines(ol, on); free_lines(cl, cn);
        WCUP_VALSTR(sel);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("find_closest_lines");
        return;
    }

    /* anchor: first line stripped; if blank, first non-blank line stripped */
    char *anchor = strip_dup(ol[0]);
    WCUP_BRANCH(anchor[0] == '\0');                            /* B3 */
    if (anchor[0] == '\0') {
        free(anchor);
        anchor = NULL;
        for (size_t i = 0; i < on; i++) {
            WCUP_LOOP_TICK();
            char *st = strip_dup(ol[i]);
            int nonblank = st[0] != '\0';
            WCUP_BRANCH(nonblank);                             /* B4 */
            if (nonblank && !anchor)
                anchor = st;
            else
                free(st);
        }
        int have_anchor = anchor != NULL;
        WCUP_BRANCH(!have_anchor);                             /* B5 */
        if (!have_anchor) {
            free_lines(ol, on); free_lines(cl, cn);
            WCUP_VALSTR(sel);
            char cup[32];
            WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup);
            WCUP_END("find_closest_lines");
            return;
        }
    }

    /* score each content line by similarity to the anchor */
    typedef struct { double r; size_t idx; } sc_t;
    sc_t *sc = NULL;
    size_t nsc = 0, scap = 0;
    size_t alen = strlen(anchor);
    for (size_t i = 0; i < cn; i++) {
        WCUP_LOOP_TICK();
        char *st = strip_dup(cl[i]);
        int blank = st[0] == '\0';
        WCUP_BRANCH(blank);                                    /* B6 */
        double ratio = 0.0;
        if (!blank) {
            ratio = seq_ratio(anchor, alen, st, strlen(st));
            int hits = ratio > 0.3;
            WCUP_BRANCH(hits);                                 /* B7 */
            if (hits) {
                if (nsc >= scap) {
                    scap = scap ? scap * 2 : 8;
                    sc = realloc(sc, scap * sizeof(sc_t));
                }
                sc[nsc].r = ratio;
                sc[nsc].idx = i;
                nsc++;
            }
        }
        free(st);
    }
    int none_scored = nsc == 0;
    WCUP_BRANCH(none_scored);                                  /* B8 */
    if (none_scored) {
        free(sc); free(anchor); free_lines(ol, on); free_lines(cl, cn);
        WCUP_VALSTR(sel);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("find_closest_lines");
        return;
    }

    /* sort desc by ratio (selection sort, mirrors the C port) */
    for (size_t i = 0; i < nsc; i++)
        for (size_t j = i + 1; j < nsc; j++)
            if (sc[j].r > sc[i].r) { sc_t t = sc[i]; sc[i] = sc[j]; sc[j] = t; }
    size_t top_n = nsc < (size_t)max_results ? nsc : (size_t)max_results;

    long *seen_s = NULL, *seen_e = NULL;
    size_t nseen = 0, sccap2 = 0;
    size_t nemitted = 0;
    for (size_t t = 0; t < top_n; t++) {
        WCUP_LOOP_TICK();
        long idx = (long)sc[t].idx;
        long start = idx - context_lines;
        if (start < 0) start = 0;
        long end = (long)cn;
        if (idx + (long)on + context_lines < end)
            end = idx + (long)on + context_lines;
        int dup = 0;
        for (size_t s = 0; s < nseen; s++)
            if (seen_s[s] == start && seen_e[s] == end) { dup = 1; break; }
        WCUP_BRANCH(dup);                                      /* B9 */
        if (dup) continue;
        if (nseen >= sccap2) {
            sccap2 = sccap2 ? sccap2 * 2 : 8;
            seen_s = realloc(seen_s, sccap2 * sizeof(long));
            seen_e = realloc(seen_e, sccap2 * sizeof(long));
        }
        seen_s[nseen] = start;
        seen_e[nseen] = end;
        nseen++;
        char one[32];
        snprintf(one, sizeof one, "%s%ld", nemitted ? "." : "", idx + 1);
        strncat(sel, one, sizeof sel - strlen(sel) - 1);
        nemitted++;
    }
    int nothing_emitted = nemitted == 0;
    WCUP_BRANCH(nothing_emitted);                              /* B10 */

    free(seen_s); free(seen_e); free(sc); free(anchor);
    free_lines(ol, on); free_lines(cl, cn);
    WCUP_VALSTR(sel);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("find_closest_lines");
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s find_closest_lines '[old,content,ctx,max]'...\n",
                argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *jerr = NULL;
        json_t *root = json_parse(argv[i], &jerr);
        if (!root || root->type != JSON_ARRAY || root->c.count < 4) {
            fprintf(stderr, "bad input: %s\n", argv[i]);
            return 2;
        }
        const char *old_s = root->c.items[0] && root->c.items[0]->type == JSON_STRING
                                ? root->c.items[0]->str_val : "";
        const char *cont = root->c.items[1] && root->c.items[1]->type == JSON_STRING
                               ? root->c.items[1]->str_val : "";
        double ctx = root->c.items[2] ? root->c.items[2]->num_val : 0.0;
        double maxr = root->c.items[3] ? root->c.items[3]->num_val : 0.0;
        trace_find_closest(old_s, cont, (int)ctx, (int)maxr);
    }
    return 0;
}
