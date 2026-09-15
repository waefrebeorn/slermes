/* gate: agent_agent_runtime_helpers_py__try_recover_primary_transport — traced copy of arh_try_recover_primary_transport */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool arh_try_recover_primary_transport(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("arh_try_recover_primary_transport");
    bool result = arh_try_recover_primary_transport(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("arh_try_recover_primary_transport");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s arh_try_recover_primary_transport <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
