/* gate: tools_environments_base_py___cwd_marker — traced copy of envb_u_cwd_marker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envb_u_cwd_marker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envb_u_cwd_marker");
    int result = envb_u_cwd_marker(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envb_u_cwd_marker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_u_cwd_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
