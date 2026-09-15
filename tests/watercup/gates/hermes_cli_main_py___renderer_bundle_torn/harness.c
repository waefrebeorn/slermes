/* gate: hermes_cli_main_py___renderer_bundle_torn — traced copy of mn_renderer_bundle_torn */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mn_renderer_bundle_torn(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mn_renderer_bundle_torn");
    bool result = mn_renderer_bundle_torn(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mn_renderer_bundle_torn");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_renderer_bundle_torn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
