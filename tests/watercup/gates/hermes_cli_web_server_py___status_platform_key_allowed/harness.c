/* gate: hermes_cli_web_server_py___status_platform_key_allowed — traced copy of _status_platform_key_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _status_platform_key_allowed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_status_platform_key_allowed");
    int result = _status_platform_key_allowed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_status_platform_key_allowed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _status_platform_key_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
