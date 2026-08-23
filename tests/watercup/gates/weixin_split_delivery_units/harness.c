/* gate: weixin_split_delivery_units — traced faithful copy of
 * src/gateway/platforms/weixin.c::weixin_split_delivery_units_for_weixin
 * (+ its weixin_split_markdown_blocks / is_fence / join_strip helpers).
 * LIVE twin: gateway/platforms/weixin.py::_split_delivery_units_for_weixin.
 * Input encoding: '|' stands for '\n' (argv-safe).
 *
 * Excluded input class (documented): whitespace-only lines — LIVE treats them
 * as blank (line.strip() empty); the C port only checks rl[0]=='\0'.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    while (len > 0 && (buf[len-1]==' '||buf[len-1]=='\t'||buf[len-1]=='\n'||buf[len-1]=='\r'))
        buf[--len] = '\0';
    const char *p = buf;
    while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
    if (p != buf) memmove(buf, p, strlen(p) + 1);
    return buf;
}

static bool is_fence(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    return strncmp(line, "```", 3) == 0;
}

static void push_unit(char ***units, int *nu, char *u) {
    **units = u; (*units)++; (*nu)++;
}

static char **g_blocks; static int g_nb;

/* split content into blocks: fences kept intact, blank lines separate */
static void split_blocks_trace(const char *content) {
    char *blocks[64]; int nb = 0;
    char *cur[64]; int nc = 0;
    bool in_code = false;
    const char *start = content;
    for (const char *p = content; ; p++) {
        if (*p == '\n' || *p == '\0') {
            char line[512];
            size_t len = p - start;
            if (len > sizeof(line)-1) len = sizeof(line)-1;
            memcpy(line, start, len); line[len] = '\0';
            while (len > 0 && line[len-1] == '\r') line[--len] = '\0';
            const char *ls = line;
            while (*ls == ' ' || *ls == '\t') ls++;
            bool fence = strncmp(ls, "```", 3) == 0;
            if (fence) {
                if (!in_code && nc > 0) { blocks[nb++] = join_strip(cur, nc); nc = 0; }
                cur[nc++] = strdup(line);
                in_code = !in_code;
                if (!in_code) { blocks[nb++] = join_strip(cur, nc); nc = 0; }
            } else if (in_code) {
                cur[nc++] = strdup(line);
            } else if (line[0] == '\0') {
                if (nc > 0) { blocks[nb++] = join_strip(cur, nc); nc = 0; }
            } else {
                cur[nc++] = strdup(line);
            }
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    if (nc > 0) blocks[nb++] = join_strip(cur, nc);
    g_blocks = blocks; g_nb = nb;
}

static char **g_blocks; static int g_nb;

static void trace_one(const char *content_in) {
    WCUP_BEGIN("split_delivery_units");
    char buf[2048];
    const char *content = content_in;
    if (content && strchr(content, '|')) {
        char *w = buf;
        for (const char *p = content; *p && w < buf + sizeof buf - 1; p++)
            *w++ = (*p == '|') ? '\n' : *p;
        *w = '\0';
        content = buf;
    }
    char *units[128]; int nu = 0;
    bool empty = !content || !*content;
    WCUP_BRANCH(empty);
    if (!empty) {
        g_blocks = NULL; g_nb = 0;
        split_blocks_trace(content);
        for (int b = 0; b < g_nb; b++) {
            const char *blk = g_blocks[b];
            WCUP_LOOP_TICK();
            const char *first = blk;
            while (*first == ' ' || *first == '\t') first++;
            if (is_fence(first)) {
                WCUP_BRANCH(true);
                units[nu++] = strdup(blk);
                continue;
            }
            WCUP_BRANCH(false);
            char **lines = malloc(sizeof(char *) * 128);
            int nl = 0;
            const char *start = blk;
            for (const char *p = blk; ; p++) {
                if (*p == '\n' || *p == '\0') {
                    size_t len = p - start;
                    char *l = malloc(len + 1);
                    memcpy(l, start, len); l[len] = '\0';
                    lines[nl++] = l;
                    start = p + 1;
                    if (*p == '\0') break;
                }
            }
            char *cur[64]; int nc = 0;
            for (int i = 0; i < nl; i++) {
                const char *rl = lines[i];
                WCUP_LOOP_TICK();
                bool blank = rl[0] == '\0';
                if (blank) {
                    WCUP_BRANCH(true);
                    WCUP_BRANCH(nc > 0);
                    if (nc > 0) { cur[nc] = NULL; units[nu++] = join_strip(cur, nc); nc = 0; }
                    continue;
                }
                WCUP_BRANCH(false);
                bool cont = (nc > 0) && (rl[0] == ' ' || rl[0] == '\t');
                if (cont) {
                    WCUP_BRANCH(true);
                    cur[nc++] = strdup(rl);
                    continue;
                }
                WCUP_BRANCH(false);
                WCUP_BRANCH(nc > 0);
                if (nc > 0) { cur[nc] = NULL; units[nu++] = join_strip(cur, nc); }
                cur[nc++] = strdup(rl);
            }
            WCUP_BRANCH(nc > 0);
            if (nc > 0) { cur[nc] = NULL; units[nu++] = join_strip(cur, nc); }
        }
        /* filter empties */
        int out = 0;
        for (int i = 0; i < nu; i++) if (units[i] && units[i][0]) units[out++] = units[i];
        nu = out;
        for (int i = 0; i < nu; i++) WCUP_VALSTR(units[i]);
    }
    char cnt[32];
    snprintf(cnt, sizeof cnt, "count=%d", nu);
    WCUP_VALSTR(cnt);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("split_delivery_units");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s split_delivery_units <'|'-encoded text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
