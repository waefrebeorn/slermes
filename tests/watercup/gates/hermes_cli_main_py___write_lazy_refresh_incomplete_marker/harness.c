/* gate: hermes_cli_main_py___write_lazy_refresh_incomplete_marker — traced copy of main_u_write_lazy_refresh_incomplete_marker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_u_write_lazy_refresh_incomplete_marker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_u_write_lazy_refresh_incomplete_marker");
    int result = main_u_write_lazy_refresh_incomplete_marker(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_u_write_lazy_refresh_incomplete_marker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_u_write_lazy_refresh_incomplete_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
