/* gate: weixin_normalize_markdown_blocks — traced C copy of
 * src/gateway/platforms/weixin.c::weixin_normalize_markdown_blocks
 * (LIVE twin: gateway/platforms/weixin.py::_normalize_markdown_blocks).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   per line loop (L ticks):
 *     V1: rstripped line
 *     B1: fence line?  -> toggle in_code, keep line, blank_run=0
 *     B2: in_code?     -> keep line
 *     B3: blank line?
 *       B4: blank_run <= 1 -> emit ""
 *     B5 (else): blank_run = 0, emit line
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static bool is_fence(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    return strncmp(line, "```", 3) == 0;
}

static void normalize(const char *content) {
    WCUP_BEGIN("normalize_markdown_blocks");
    char out[8192];
    out[0] = '\0';
    size_t olen = 0;
    bool in_code = false;
    int blank_run = 0;

    size_t clen = content ? strlen(content) : 0;
    /* split on '\n' like str.split("\n") semantics of splitlines for \n-only
     * input: no trailing empty segment, empty string -> zero lines */
    int first = 1;
    size_t ipos = 0;
    while (ipos < clen) {
        {
            const char *start = (content ? content : "") + ipos;
            const char *p = start;
            while (*p && *p != '\n') p++;
            size_t len = (size_t)(p - start);
            char line[2048];
            if (len >= sizeof(line)) len = sizeof(line) - 1;
            memcpy(line, start, len); line[len] = '\0';
            WCUP_LOOP_TICK();
            /* rstrip whitespace */
            size_t ll = strlen(line);
            while (ll > 0 && (line[ll-1]==' '||line[ll-1]=='\t'||line[ll-1]=='\r'||line[ll-1]=='\v'||line[ll-1]=='\f')) line[--ll] = '\0';
            WCUP_VALSTR(line);                           /* V1 */
            if (is_fence(line)) {                        /* B1 */
                WCUP_BRANCH(1);
                in_code = !in_code;
                blank_run = 0;
                if (!first) { out[olen++]='\n'; }
                memcpy(out+olen, line, ll+1); olen += ll;
                first = 0;
            } else {
                WCUP_BRANCH(0);
                if (in_code) {                           /* B2 */
                    WCUP_BRANCH(1);
                    if (!first) { out[olen++]='\n'; }
                    memcpy(out+olen, line, ll+1); olen += ll;
                    first = 0;
                } else {
                    WCUP_BRANCH(0);
                    if (line[0] == '\0') {               /* B3 */
                        WCUP_BRANCH(1);
                        blank_run++;
                        if (blank_run <= 1) {            /* B4 */
                            WCUP_BRANCH(1);
                            if (!first) { out[olen++]='\n'; }
                            first = 0;
                        } else {
                            WCUP_BRANCH(0);
                        }
                    } else {
                        WCUP_BRANCH(0);                  /* B5 */
                        blank_run = 0;
                        if (!first) { out[olen++]='\n'; }
                        memcpy(out+olen, line, ll+1); olen += ll;
                        first = 0;
                    }
                }
            }
            ipos += len + 1;  /* skip past the '\n' (or nul terminator) */
        }
    }
    /* final .strip() */
    {
        size_t b = 0, L = olen;
        while (b < L && (out[b]=='\n'||out[b]=='\r'||out[b]=='\t'||out[b]==' '||out[b]=='\v'||out[b]=='\f')) b++;
        while (L > b && (out[L-1]=='\n'||out[L-1]=='\r'||out[L-1]=='\t'||out[L-1]==' '||out[L-1]=='\v'||out[L-1]=='\f')) L--;
        memmove(out, out + b, L - b);
        out[L-b] = '\0';
    }
    WCUP_VALSTR(out);                                    /* V2 result */
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("normalize_markdown_blocks");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize <content>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) normalize(argv[i]);
    return 0;
}
