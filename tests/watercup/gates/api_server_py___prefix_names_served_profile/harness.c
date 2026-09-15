/* gate: api_server_py___prefix_names_served_profile — traced copy of apisg_prefix_names_served_profile */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool apisg_prefix_names_served_profile(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apisg_prefix_names_served_profile");
    bool result = apisg_prefix_names_served_profile(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apisg_prefix_names_served_profile");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apisg_prefix_names_served_profile <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
