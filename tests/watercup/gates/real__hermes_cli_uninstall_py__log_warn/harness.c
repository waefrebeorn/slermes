/* gate: real__hermes_cli_uninstall_py__log_warn — GAP sentinel for log_warn */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "log_warn";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s log_warn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("log_warn");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("log_warn");
    }
    return 0;
}
