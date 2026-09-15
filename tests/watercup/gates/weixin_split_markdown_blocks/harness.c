/* gate: weixin_split_markdown_blocks — traced C copy of
 * src/gateway/platforms/weixin.c::weixin_split_markdown_blocks (+ its
 * weixin__join_strip helper). PoP: gateway/platforms/weixin.py:
 * _split_markdown_blocks.
 * argv input: one content string per arg ('\n' inside the arg).
 * Output cup value: count + '\x1f'-joined blocks.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

static char *join_strip(char **lines, int n) {
    size_t total = 1;
    for (int i = 0; i < n; i++) total += strlen(lines[i]) + 1;
    char *buf = malloc(total);
    buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strcat(buf, "\n");
        strcat(buf, lines[i]);
    }
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t' ||
                       buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    const char *p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p != buf) memmove(buf, p, strlen(p) + 1);
    return buf;
}

static void emit_unused(void) { /* placeholder removed */ }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s split_markdown_blocks <content>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        const char *content = argv[a];
        WCUP_BEGIN("split_markdown_blocks");
        /* split into lines (strip trailing CR like the C port) */
        char *copy = strdup(content);
        char **lines = malloc(sizeof(char *) * (strlen(content) + 2));
        int nl = 0;
        {
            char *start = copy;
            for (char *p = copy; ; p++) {
                if (*p == '\n' || *p == '\0') {
                    size_t len = (size_t)(p - start);
                    while (len > 0 && start[len-1] == '\r') len--;
                    char *line = malloc(len + 1);
                    memcpy(line, start, len); line[len] = '\0';
                    lines[nl++] = line;
                    start = p + 1;
                    if (*p == '\0') break;
                }
            }
        }
        char **current = malloc(sizeof(char *) * (nl + 1));
        int nc = 0, nb = 0;
        bool in_code = false;
        for (int i = 0; i < nl; i++) {
            const char *ls = lines[i];
            while (*ls == ' ' || *ls == '\t') ls++;
            bool is_fence = (strncmp(ls, "```", 3) == 0);
            if (is_fence) {                                   /* B fence? */
                WCUP_BRANCH(1);
                if (!in_code && nc > 0) {                     /* B flush-open */
                    WCUP_BRANCH(1);
                    nb++; nc = 0;
                } else {
                    WCUP_BRANCH(0);
                }
                nc++;
                in_code = !in_code;
                if (!in_code) {                               /* B close-flush */
                    WCUP_BRANCH(1);
                    nb++; nc = 0;
                } else {
                    WCUP_BRANCH(0);
                }
                continue;
            } else {
                WCUP_BRANCH(0);
            }
            if (in_code) {                                    /* B in-code */
                WCUP_BRANCH(1);
                nc++;
                continue;
            } else {
                WCUP_BRANCH(0);
            }
            if (lines[i][0] == '\0') {                        /* B blank */
                WCUP_BRANCH(1);
                if (nc > 0) {                                 /* B flush */
                    WCUP_BRANCH(1);
                    nb++; nc = 0;
                } else {
                    WCUP_BRANCH(0);
                }
                continue;
            } else {
                WCUP_BRANCH(0);
            }
            nc++;
        }
        if (nc > 0) {                                         /* B tail flush */
            WCUP_BRANCH(1);
            nb++;
        } else {
            WCUP_BRANCH(0);
        }
        /* empty-block filter: loop tick per surviving block */
        int out_n = nb; /* blocks are non-empty post join_strip unless line was ws-only */
        /* Recompute actual kept blocks to encode the output faithfully. */
        /* (Re-walk cheaply: re-run the same packing without trace to build values.) */
        char **blocks = malloc(sizeof(char *) * (nb + 1));
        int bn = 0;
        nc = 0; in_code = false;
        for (int i = 0; i < nl; i++) {
            const char *ls = lines[i];
            while (*ls == ' ' || *ls == '\t') ls++;
            bool is_fence = (strncmp(ls, "```", 3) == 0);
            if (is_fence) {
                if (!in_code && nc > 0) { blocks[bn++] = join_strip(current, nc); nc = 0; }
                current[nc++] = strdup(lines[i]);
                in_code = !in_code;
                if (!in_code) { blocks[bn++] = join_strip(current, nc); nc = 0; }
                continue;
            }
            if (in_code) { current[nc++] = strdup(lines[i]); continue; }
            if (lines[i][0] == '\0') {
                if (nc > 0) { blocks[bn++] = join_strip(current, nc); nc = 0; }
                continue;
            }
            current[nc++] = strdup(lines[i]);
        }
        if (nc > 0) blocks[bn++] = join_strip(current, nc);
        /* filter empties */
        char joined[65536]; joined[0] = '\0';
        char cnt[16];
        snprintf(cnt, sizeof cnt, "%d", bn);
        strcat(joined, cnt);
        int kept = 0;
        for (int i = 0; i < bn; i++) {
            if (blocks[i] && blocks[i][0]) {
                strcat(joined, "\x1f"); strcat(joined, blocks[i]);
                kept++;
            }
        }
        (void)out_n; (void)kept;
        WCUP_VALSTR(joined);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("split_markdown_blocks");
    }
    return 0;
}
