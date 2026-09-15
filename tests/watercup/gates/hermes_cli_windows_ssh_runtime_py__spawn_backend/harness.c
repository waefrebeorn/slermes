/* gate: hermes_cli_windows_ssh_runtime_py__spawn_backend — traced copy of wssr_spawn_backend */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wssr_spawn_backend(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wssr_spawn_backend");
    int result = wssr_spawn_backend(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wssr_spawn_backend");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wssr_spawn_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
