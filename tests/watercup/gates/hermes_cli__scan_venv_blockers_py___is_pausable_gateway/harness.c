/* gate: hermes_cli__scan_venv_blockers_py___is_pausable_gateway — traced copy of scanvb_is_pausable_gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool scanvb_is_pausable_gateway(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scanvb_is_pausable_gateway");
    bool result = scanvb_is_pausable_gateway(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scanvb_is_pausable_gateway");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scanvb_is_pausable_gateway <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
