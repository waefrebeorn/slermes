/* gate: agent_secret_sources_base_py__is_valid_env_name — traced copy of is_valid_env_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_valid_env_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_valid_env_name");
    bool result = is_valid_env_name(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_valid_env_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_valid_env_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
