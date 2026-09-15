/* gate: hermes_cli_heartbeat_py___UNIT_SECONDS — traced copy of hb_unit_seconds */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hb_unit_seconds(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hb_unit_seconds");
    int result = hb_unit_seconds(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hb_unit_seconds");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hb_unit_seconds <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
