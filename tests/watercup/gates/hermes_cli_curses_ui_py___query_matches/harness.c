/* gate: hermes_cli_curses_ui_py___query_matches — traced copy of curses_query_matches */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int curses_query_matches(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("curses_query_matches");
    int result = curses_query_matches(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("curses_query_matches");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s curses_query_matches <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
