/* gate: yuanbao_sanitize_markdown_table — traced C copy of
 * src/gateway/platforms/yuanbao.c::yuanbao_md_sanitize_markdown_table.
 * Twin: LIVE Python gateway/platforms/yuanbao.py::YuanbaoPlatform.sanitize_markdown_table.
 * Trace contract (matches gates/<g>/twin.py):
 *   b0: text contains '|'
 *   per line: LOOP tick
 *   b1: stripped line is a table row (|...|)
 *   b2: separator row
 *   b3 (only when !b2): empty/pipes-only row skip  (branch(False) when len>2)
 * Single V event: the final sanitized string.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static int is_sep_row(const char *ls, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = ls[i];
        if (c != '-' && c != ':' && c != ' ' && c != '\t' && c != '|') return 0;
    }
    return 1;
}

static void append(char *res, size_t *rlen, const char *s, size_t n) {
    memcpy(res + *rlen, s, n);
    *rlen += n;
}

/* normalize a separator row the way both sides do: split on '|', strip each
 * cell, drop edge empties, rejoin with '|' and leading/trailing pipes */
static void normalize_sep(const char *ls, size_t n, char *out, size_t *olen) {
    char *scopy = strndup(ls, n);
    size_t cl = 0;
    out[cl++] = '|';
    char *save = NULL, *tok = strtok_r(scopy, "|", &save);
    int first = 1;
    while (tok) {
        char *ts = tok; while (*ts == ' ' || *ts == '\t') ts++;
        char *te = ts + strlen(ts);
        while (te > ts && (te[-1] == ' ' || te[-1] == '\t')) te--;
        if (!first) out[cl++] = '|';
        memcpy(out + cl, ts, (size_t)(te - ts)); cl += (size_t)(te - ts);
        first = 0;
        tok = strtok_r(NULL, "|", &save);
    }
    out[cl++] = '|'; out[cl] = '\0';
    *olen = cl;
    free(scopy);
}

static char *sanitize(const char *text) {
    WCUP_BEGIN("sanitize_markdown_table");
    char *result = malloc(strlen(text) + 1024);
    size_t rlen = 0;
    result[0] = '\0';
    WCUP_BRANCH(strchr(text, '|') != NULL);                 /* b0 */
    if (!strchr(text, '|')) {
        strcpy(result, text);
    } else {
        /* iterate lines without a final phantom empty line after '\n' */
        const char *p = text;
        while (*p) {
            WCUP_LOOP_TICK();                                /* per line */
            const char *eol = strchr(p, '\n');
            size_t ll = eol ? (size_t)(eol - p) : strlen(p);
            /* strip() the line */
            const char *ls = p;
            while (ls < p + ll && (*ls == ' ' || *ls == '\t')) ls++;
            size_t ls_len = (p + ll) - ls;
            while (ls_len > 0 && (ls[ls_len-1]==' '||ls[ls_len-1]=='\t'||ls[ls_len-1]=='\r'||ls[ls_len-1]=='\n')) ls_len--;
            int is_table_row = (ls_len >= 2 && ls[0] == '|' && ls[ls_len-1] == '|');
            WCUP_BRANCH(is_table_row);                       /* b1 */
            if (is_table_row) {
                int is_sep = is_sep_row(ls, ls_len);
                WCUP_BRANCH(is_sep);                         /* b2 */
                if (is_sep) {
                    char norm[512]; size_t nlen = 0;
                    normalize_sep(ls, ls_len, norm, &nlen);
                    append(result, &rlen, norm, nlen);
                } else {
                    int only_pipe_ws = 0;
                    if (ls_len <= 2) {
                        only_pipe_ws = 1;
                        for (size_t i = 0; i < ls_len; i++) {
                            if (ls[i] != '|' && !isspace((unsigned char)ls[i])) { only_pipe_ws = 0; break; }
                        }
                        WCUP_BRANCH(only_pipe_ws);           /* b3 (skip when true) */
                        if (!only_pipe_ws) append(result, &rlen, ls, ls_len);
                    } else {
                        WCUP_BRANCH(0);                      /* b3 not taken */
                        append(result, &rlen, ls, ls_len);
                    }
                }
            } else {
                append(result, &rlen, p, ll);
            }
            if (eol) { result[rlen++] = '\n'; p = eol + 1; }
            else break;
        }
        result[rlen] = '\0';
    }
    WCUP_VALSTR(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sanitize_markdown_table");
    free(result);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sanitize_markdown_table <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) sanitize(argv[i]);
    return 0;
}
