/* gate: tools_skills_tool_py___is_remote_env_backend — traced copy of skt_is_remote_env_backend */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skt_is_remote_env_backend(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skt_is_remote_env_backend");
    int result = skt_is_remote_env_backend(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skt_is_remote_env_backend");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skt_is_remote_env_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
