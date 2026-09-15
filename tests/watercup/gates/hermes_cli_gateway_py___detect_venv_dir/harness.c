/* gate: hermes_cli_gateway_py___detect_venv_dir — traced copy of cgw_u_detect_venv_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cgw_u_detect_venv_dir(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cgw_u_detect_venv_dir");
    int result = cgw_u_detect_venv_dir(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cgw_u_detect_venv_dir");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cgw_u_detect_venv_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
