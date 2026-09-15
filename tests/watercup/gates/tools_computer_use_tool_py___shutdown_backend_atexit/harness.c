/* gate: tools_computer_use_tool_py___shutdown_backend_atexit — traced copy of cut_shutdown_backend_atexit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cut_shutdown_backend_atexit(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cut_shutdown_backend_atexit");
    int result = cut_shutdown_backend_atexit(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cut_shutdown_backend_atexit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cut_shutdown_backend_atexit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
