/* gate: gateway_channel_directory_py___build_from_sessions_json — traced copy of gateway_channel_directory_u_build_from_sessions_json */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_channel_directory_u_build_from_sessions_json(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_channel_directory_u_build_from_sessions_json");
    int result = gateway_channel_directory_u_build_from_sessions_json(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_channel_directory_u_build_from_sessions_json");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_channel_directory_u_build_from_sessions_json <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
