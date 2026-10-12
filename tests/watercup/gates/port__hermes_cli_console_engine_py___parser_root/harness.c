/* gate: port__hermes_cli_console_engine_py___parser_root — GAP sentinel for _parser_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_parser_root";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _parser_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_parser_root");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_parser_root");
    }
    return 0;
}
