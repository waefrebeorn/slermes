/* gate: hermes_cli_banner_py___is_ssh_remote — traced copy of banner_is_ssh_remote */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool banner_is_ssh_remote(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("banner_is_ssh_remote");
    bool result = banner_is_ssh_remote(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("banner_is_ssh_remote");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s banner_is_ssh_remote <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
