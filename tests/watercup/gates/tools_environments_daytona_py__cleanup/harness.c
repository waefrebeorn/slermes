/* gate: tools_environments_daytona_py__cleanup — traced copy of daytona_stop_sandbox */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool daytona_stop_sandbox(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("daytona_stop_sandbox");
    bool result = daytona_stop_sandbox(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("daytona_stop_sandbox");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s daytona_stop_sandbox <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
