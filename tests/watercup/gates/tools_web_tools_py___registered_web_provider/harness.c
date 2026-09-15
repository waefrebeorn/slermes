/* gate: tools_web_tools_py___registered_web_provider — traced copy of tools_web_tools_u_registered_web_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_web_tools_u_registered_web_provider(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_web_tools_u_registered_web_provider");
    int result = tools_web_tools_u_registered_web_provider(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_web_tools_u_registered_web_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_web_tools_u_registered_web_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
