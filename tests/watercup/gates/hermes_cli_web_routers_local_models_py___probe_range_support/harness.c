/* gate: hermes_cli_web_routers_local_models_py___probe_range_support — traced copy of _probe_range_support */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _probe_range_support(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_probe_range_support");
    int result = _probe_range_support(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_probe_range_support");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _probe_range_support <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
