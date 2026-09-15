/* gate: tools_computer_use_cua_backend_py__capabilities_discovered — traced copy of cua_capabilities_discovered */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cua_capabilities_discovered(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cua_capabilities_discovered");
    int result = cua_capabilities_discovered(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cua_capabilities_discovered");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cua_capabilities_discovered <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
