/* gate: hermes_cli_heartbeat_py__migrate_heartbeat_to_session — traced copy of hb_migrate_heartbeat_to_session */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hb_migrate_heartbeat_to_session(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hb_migrate_heartbeat_to_session");
    int result = hb_migrate_heartbeat_to_session(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hb_migrate_heartbeat_to_session");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hb_migrate_heartbeat_to_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
