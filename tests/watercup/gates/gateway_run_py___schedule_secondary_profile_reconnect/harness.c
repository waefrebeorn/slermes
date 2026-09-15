/* gate: gateway_run_py___schedule_secondary_profile_reconnect — traced copy of grun_u_schedule_secondary_profile_reconnect */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_schedule_secondary_profile_reconnect(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_schedule_secondary_profile_reconnect");
    int result = grun_u_schedule_secondary_profile_reconnect(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_schedule_secondary_profile_reconnect");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_schedule_secondary_profile_reconnect <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
