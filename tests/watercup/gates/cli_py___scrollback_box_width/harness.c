/* gate: cli_py___scrollback_box_width — traced copy of cli_scrollback_box_width */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_scrollback_box_width(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_scrollback_box_width");
    int result = cli_scrollback_box_width(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_scrollback_box_width");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_scrollback_box_width <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
