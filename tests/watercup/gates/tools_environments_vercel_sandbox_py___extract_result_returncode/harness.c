/* gate: tools_environments_vercel_sandbox_py___extract_result_returncode — traced copy of ve_extract_result_returncode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ve_extract_result_returncode(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ve_extract_result_returncode");
    int result = ve_extract_result_returncode(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ve_extract_result_returncode");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ve_extract_result_returncode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
