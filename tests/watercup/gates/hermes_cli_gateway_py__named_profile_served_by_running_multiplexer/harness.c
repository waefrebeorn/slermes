/* gate: hermes_cli_gateway_py__named_profile_served_by_running_multiplexer — traced copy of named_profile_served_by_running_multiplexer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int named_profile_served_by_running_multiplexer(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("named_profile_served_by_running_multiplexer");
    int result = named_profile_served_by_running_multiplexer(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("named_profile_served_by_running_multiplexer");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s named_profile_served_by_running_multiplexer <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
