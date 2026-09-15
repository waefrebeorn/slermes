/* gate: hermes_cli_curses_ui_py__should_replay — traced copy of curses_ui_should_replay */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int curses_ui_should_replay(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("curses_ui_should_replay");
    int result = curses_ui_should_replay(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("curses_ui_should_replay");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s curses_ui_should_replay <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
