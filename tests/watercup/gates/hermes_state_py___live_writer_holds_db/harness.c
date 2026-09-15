/* gate: hermes_state_py___live_writer_holds_db — traced copy of _live_writer_holds_db */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _live_writer_holds_db(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_live_writer_holds_db");
    bool result = _live_writer_holds_db(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_live_writer_holds_db");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _live_writer_holds_db <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
