/* gate: platforms_helpers_split_markdown_atoms — drives the REAL
 * src/gateway/platforms/platforms_helpers_md.c::helpers_md_split_markdown_atoms
 * (PoP: gateway/platforms/helpers.py::split_markdown_atoms).
 *
 * Input: markdown text with literal "\n" two-char escapes for newlines.
 *
 * Per-line rung ladder (must match twin.py EXACTLY):
 *   B1 in_fence -> [B2 fence close: ``` and >1 current lines]
 *   B3 fence open
 *   B4 table row -> [B5 prev non-table flush]
 *   B6 blank
 *   B7 plain: prev is table (flush)
 * final flush; V(atoms joined with US \x1f, newlines escaped)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

char **helpers_md_split_markdown_atoms(const char *text);

static void unescape(char *s) {
    char *w = s;
    for (char *r = s; *r;) {
        if (r[0] == '\\' && r[1] == 'n') { *w++ = '\n'; r += 2; }
        else *w++ = *r++;
    }
    *w = '\0';
}

static int is_blank(const char *cur, size_t ll) {
    for (size_t i = 0; i < ll; i++)
        if (cur[i] != ' ' && cur[i] != '\t') return 0;
    return 1;
}

static int is_table_line(const char *cur, size_t ll) {
    const char *st = cur;
    while (*st == ' ' || *st == '\t') st++;
    return *st == '|' && ll > 0 && cur[ll - 1] == '|';
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_split_markdown_atoms <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char text[4096]; snprintf(text, sizeof text, "%s", argv[i]);
        unescape(text);

        WCUP_BEGIN("split_markdown_atoms");
        char *dup = strdup(text);
        int in_fence = 0, ncur = 0, prev_is_table = 0;
        char *line = dup;
        while (line) {
            char *eol = strchr(line, '\n');
            size_t ll = eol ? (size_t)(eol - line) : strlen(line);
            char cur[1024];
            memcpy(cur, line, ll); cur[ll] = '\0';
            line = eol ? eol + 1 : NULL;
            int is_table = is_table_line(cur, ll);

            WCUP_BRANCH(in_fence);                                    /* B1 */
            if (in_fence) {
                ncur++;
                prev_is_table = 0;
                WCUP_BRANCH(strncmp(cur, "```", 3) == 0 && ncur > 1); /* B2 */
                if (strncmp(cur, "```", 3) == 0 && ncur > 1) {
                    in_fence = 0; ncur = 0;
                }
            } else if (strncmp(cur, "```", 3) == 0) {                 /* B3 */
                WCUP_BRANCH(1);
                in_fence = 1; ncur = 1; prev_is_table = 0;
            } else {
                WCUP_BRANCH(0);
                WCUP_BRANCH(is_table);                                /* B4 */
                if (is_table) {
                    WCUP_BRANCH(ncur > 0 && !prev_is_table);          /* B5 */
                    if (ncur > 0 && !prev_is_table) ncur = 0;
                    ncur++; prev_is_table = 1;
                } else if (is_blank(cur, ll)) {                       /* B6 blank */
                    WCUP_BRANCH(1);
                    if (ncur > 0) ncur = 0;
                    prev_is_table = 0;
                } else {
                    WCUP_BRANCH(0);
                    WCUP_BRANCH(ncur > 0 && prev_is_table);           /* B7 */
                    if (ncur > 0 && prev_is_table) ncur = 0;
                    ncur++; prev_is_table = 0;
                }
            }
        }
        free(dup);

        char **atoms = helpers_md_split_markdown_atoms(text);
        char vs[2048]; int off = 0;
        if (atoms) {
            for (int k = 0; atoms[k]; k++) {
                if (k && off < 2044) vs[off++] = '\x1f';
                for (const char *q = atoms[k]; *q && off < 2044; q++) {
                    if (*q == '\n' && off < 2042) { vs[off++] = '\\'; vs[off++] = 'n'; }
                    else vs[off++] = *q;
                }
            }
        }
        vs[off] = '\0';
        WCUP_VALSTR(vs);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("split_markdown_atoms");
    }
    return 0;
}
