/* gate: tools_web_tools_py___is_backend_available — traced copy of web_is_backend_available */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool web_is_backend_available(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("web_is_backend_available");
    bool result = web_is_backend_available(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("web_is_backend_available");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s web_is_backend_available <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
