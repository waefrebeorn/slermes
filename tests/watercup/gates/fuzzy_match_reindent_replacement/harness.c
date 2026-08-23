/* gate: fuzzy_match_reindent_replacement — traced copy of
 * src/hermes_cli/fuzzy_match_helpers.c::fuzzy_match_reindent_replacement
 * (LIVE twin: tools/fuzzy_match.py::_reindent_replacement).
 *
 * The leading-whitespace / first-meaningful-line helpers are LINKED from the
 * real port (extra_src: src/hermes_cli/fuzzy_match_helpers.c) so this harness
 * exercises REAL C logic; only WCUP instrumentation is added here.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B0: new_string empty                     -> return unchanged
 *   V : old_first meaningful line
 *   B1: old_first or file_first missing      -> return unchanged
 *   V : file_first meaningful line
 *   V : old_indent (ws prefix string)
 *   B2: old_indent == file_indent            -> return unchanged
 *   per new_string line: L tick, then
 *     B3: blank -> keep verbatim / not-blank
 *     B4: indent startswith old_indent -> swap base prefix, else anchor
 *   V : final joined result
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

size_t fuzzy_match_leading_whitespace(const char *line);
char *fuzzy_match_first_meaningful_line(const char *text);

typedef struct { char *s; size_t len, cap; } wds_t;
static void wds_init(wds_t *d) { d->s = NULL; d->len = 0; d->cap = 0; }
static void wds_append(wds_t *d, const char *txt, size_t n) {
    if (d->len + n + 1 > d->cap) {
        d->cap = (d->len + n + 1) * 2;
        d->s = (char *)realloc(d->s, d->cap);
    }
    memcpy(d->s + d->len, txt, n);
    d->len += n;
    d->s[d->len] = '\0';
}

static char *wc_reindent(const char *file_region, const char *old_string,
                         const char *new_string) {
    char *result = NULL;
    char *old_first = NULL, *file_first = NULL;
    size_t old_ind_n = 0, file_ind_n = 0;
    WCUP_BEGIN("fuzzy_match_reindent_replacement");

    if (!new_string || !*new_string) {
        WCUP_BRANCH(1);                       /* B0 */
        result = strdup(new_string ? new_string : "");
        goto out;
    }
    WCUP_BRANCH(0);

    old_first = fuzzy_match_first_meaningful_line(old_string);
    file_first = fuzzy_match_first_meaningful_line(file_region);
    WCUP_VALSTR(old_first ? old_first : "<none>");
    if (!old_first || !file_first) {
        WCUP_BRANCH(1);                       /* B1 */
        result = strdup(new_string);
        goto out;
    }
    WCUP_BRANCH(0);
    WCUP_VALSTR(file_first);

    old_ind_n = fuzzy_match_leading_whitespace(old_first);
    file_ind_n = fuzzy_match_leading_whitespace(file_first);
    {
        char indbuf[8];
        snprintf(indbuf, sizeof indbuf, "%zu", old_ind_n);
        WCUP_VALSTR(indbuf);                  /* old_indent (count form) */
    }
    if (old_ind_n == file_ind_n &&
        strncmp(old_first, file_first, old_ind_n) == 0) {
        WCUP_BRANCH(1);                       /* B2 */
        result = strdup(new_string);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        wds_t out;
        const char *p = new_string;
        int first_line = 1;
        wds_init(&out);
        while (1) {
            const char *nl = strchr(p, '\n');
            size_t llen = nl ? (size_t)(nl - p) : strlen(p);
            int blank = 1;
            size_t j;
            WCUP_LOOP_TICK();
            for (j = 0; j < llen; j++)
                if (p[j] != ' ' && p[j] != '\t') { blank = 0; break; }
            if (blank) {
                WCUP_BRANCH(1);               /* B3 blank */
                if (!first_line) wds_append(&out, "\n", 1);
                wds_append(&out, p, llen);
            } else {
                size_t li_n = fuzzy_match_leading_whitespace(p);
                WCUP_BRANCH(0);               /* B3 not-blank */
                if (li_n >= old_ind_n &&
                    strncmp(p, old_first, old_ind_n) == 0) {
                    WCUP_BRANCH(1);           /* B4 swap base prefix */
                    if (!first_line) wds_append(&out, "\n", 1);
                    wds_append(&out, file_first, file_ind_n);
                    wds_append(&out, p + old_ind_n, llen - old_ind_n);
                } else {
                    WCUP_BRANCH(0);           /* B4 anchor to file base */
                    j = 0;
                    while (j < llen && (p[j] == ' ' || p[j] == '\t')) j++;
                    if (!first_line) wds_append(&out, "\n", 1);
                    wds_append(&out, file_first, file_ind_n);
                    wds_append(&out, p + j, llen - j);
                }
            }
            first_line = 0;
            if (!nl) break;
            p = nl + 1;
        }
        result = out.s ? out.s : strdup("");
    }
out:
    free(old_first);
    free(file_first);
    WCUP_VALSTR(result ? result : "");
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fuzzy_match_reindent_replacement");
        return result;
    }
}

/* each argv input is one case: region\x1f old\x1f new */
int main(int argc, char **argv) {
    int i;
    if (argc < 3) {
        fprintf(stderr, "usage: %s wc_reindent <region\\x1fold\\x1fnew> ...\n", argv[0]);
        return 2;
    }
    for (i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *old_s = strchr(arg, '\x1f');
        char *new_s = NULL;
        if (old_s) { *old_s = '\0'; old_s++;
            new_s = strchr(old_s, '\x1f');
            if (new_s) { *new_s = '\0'; new_s++; }
        }
        char *r = wc_reindent(arg, old_s ? old_s : "", new_s ? new_s : "");
        free(r);
        free(arg);
    }
    return 0;
}
