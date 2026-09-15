/* gate: hermes_cli_web_routers_local_models_py___engine_too_old — traced copy of _engine_too_old */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _engine_too_old(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_engine_too_old");
    int result = _engine_too_old(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_engine_too_old");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _engine_too_old <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
