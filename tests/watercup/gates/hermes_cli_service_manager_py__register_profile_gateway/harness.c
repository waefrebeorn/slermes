/* gate: hermes_cli_service_manager_py__register_profile_gateway — traced copy of sm_register_profile_gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sm_register_profile_gateway(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("sm_register_profile_gateway");
    int result = sm_register_profile_gateway(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sm_register_profile_gateway");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sm_register_profile_gateway <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
