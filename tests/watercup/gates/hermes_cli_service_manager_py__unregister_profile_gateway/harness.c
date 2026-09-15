/* gate: hermes_cli_service_manager_py__unregister_profile_gateway — traced copy of sm_unregister_profile_gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sm_unregister_profile_gateway(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sm_unregister_profile_gateway");
    int result = sm_unregister_profile_gateway(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sm_unregister_profile_gateway");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sm_unregister_profile_gateway <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
