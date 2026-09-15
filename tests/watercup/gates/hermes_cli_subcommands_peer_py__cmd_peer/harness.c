/* gate: hermes_cli_subcommands_peer_py__cmd_peer — traced copy of peer_cmd_peer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int peer_cmd_peer(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("peer_cmd_peer");
    int result = peer_cmd_peer(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("peer_cmd_peer");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s peer_cmd_peer <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
