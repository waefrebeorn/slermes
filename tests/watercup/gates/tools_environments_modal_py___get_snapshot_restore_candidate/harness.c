/* gate: tools_environments_modal_py___get_snapshot_restore_candidate — traced copy of tools_environments_modal_u_get_snapshot_restore_candidate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_environments_modal_u_get_snapshot_restore_candidate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_environments_modal_u_get_snapshot_restore_candidate");
    int result = tools_environments_modal_u_get_snapshot_restore_candidate(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_environments_modal_u_get_snapshot_restore_candidate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_environments_modal_u_get_snapshot_restore_candidate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
