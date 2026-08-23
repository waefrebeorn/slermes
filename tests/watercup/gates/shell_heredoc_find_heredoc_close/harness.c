/* gate: shell_heredoc_find_heredoc_close — traced C copy of
 * src/tools/shell_heredoc.c::find_heredoc_close
 * (LIVE twin: tools/shell_heredoc.py::_find_heredoc_close).
 *
 * Input encoding (argv-safe): "<command>\t<body_start>\t<delimiter>\t<flag>"
 *   flag = "0" | "1" for strip_tabs.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   loop per line:
 *     B1: newline found?
 *     B2: line ends with \r  (implicit: trim; V of trimmed line)
 *     B3: strip_tabs?       (V of candidate)
 *     B4: candidate == delimiter -> return after
 *     B5: newline == -1 -> return None
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

#define WC_NONE ((size_t)-1)

static size_t find_heredoc_close(const char *command, size_t body_start,
                                 const char *delimiter, int strip_tabs) {
    WCUP_BEGIN("find_heredoc_close");
    size_t n = strlen(command);
    size_t cursor = body_start;
    size_t result = WC_NONE;
    while (1) {
        size_t newline = cursor;
        while (newline < n && command[newline] != '\n') newline++;
        int have_nl = (newline < n);
        WCUP_BRANCH(have_nl);                       /* B1 */
        size_t after = have_nl ? newline + 1 : n;
        size_t line_len = (have_nl ? newline : n) - cursor;
        char line[4096];
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, command + cursor, line_len);
        line[line_len] = '\0';
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
            WCUP_BRANCH(1);                          /* B2 */
        } else {
            WCUP_BRANCH(0);
        }
        const char *candidate = line;
        if (strip_tabs) {
            while (*candidate == '\t') candidate++;
            WCUP_BRANCH(1);                          /* B3 */
        } else {
            WCUP_BRANCH(0);
        }
        WCUP_VALSTR(candidate);                      /* V1 */
        int match = (strcmp(candidate, delimiter) == 0);
        WCUP_BRANCH(match);                          /* B4 */
        if (match) { result = after; break; }
        WCUP_BRANCH(!have_nl);                       /* B5 */
        if (!have_nl) { result = WC_NONE; break; }
        cursor = after;
    }
    char rbuf[32];
    if (result == WC_NONE) snprintf(rbuf, sizeof rbuf, "None");
    else snprintf(rbuf, sizeof rbuf, "%zu", result);
    WCUP_VALSTR(rbuf);                               /* V2 result */
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("find_heredoc_close");
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s find_heredoc_close <cmd\\tstart\\tdelim\\tflag>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *t1 = strchr(a, '\x1f');
        const char *t2 = t1 ? strchr(t1 + 1, '\x1f') : NULL;
        const char *t3 = t2 ? strchr(t2 + 1, '\x1f') : NULL;
        if (!t3) { fprintf(stderr, "bad input encoding: %s\n", a); return 2; }
        char cmd[2048]; size_t cl = (size_t)(t1 - a);
        if (cl >= sizeof(cmd)) cl = sizeof(cmd) - 1;
        memcpy(cmd, a, cl); cmd[cl] = '\0';
        size_t body_start = (size_t)strtoul(t1 + 1, NULL, 10);
        char delim[256]; size_t dl = (size_t)(t2 ? t3 - t2 - 1 : 0);
        if (dl >= sizeof(delim)) dl = sizeof(delim) - 1;
        memcpy(delim, t2 + 1, dl); delim[dl] = '\0';
        int strip_tabs = (t3[1] == '1');
        find_heredoc_close(cmd, body_start, delim, strip_tabs);
    }
    return 0;
}
