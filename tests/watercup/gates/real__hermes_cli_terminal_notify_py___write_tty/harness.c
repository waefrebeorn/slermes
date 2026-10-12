/* gate: real__hermes_cli_terminal_notify_py___write_tty — GAP sentinel for _write_tty */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_write_tty";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _write_tty <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_write_tty");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_write_tty");
    }
    return 0;
}
