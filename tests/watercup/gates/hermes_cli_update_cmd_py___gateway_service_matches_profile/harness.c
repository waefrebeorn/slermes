/* gate: hermes_cli_update_cmd_py___gateway_service_matches_profile — traced copy of _gateway_service_matches_profile */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _gateway_service_matches_profile(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_gateway_service_matches_profile");
    int result = _gateway_service_matches_profile(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_gateway_service_matches_profile");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _gateway_service_matches_profile <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
