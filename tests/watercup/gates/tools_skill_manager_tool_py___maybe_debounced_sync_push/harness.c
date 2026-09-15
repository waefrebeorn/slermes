/* gate: tools_skill_manager_tool_py___maybe_debounced_sync_push — traced copy of skillmgr_maybe_debounced_sync_push */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skillmgr_maybe_debounced_sync_push(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skillmgr_maybe_debounced_sync_push");
    int result = skillmgr_maybe_debounced_sync_push(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skillmgr_maybe_debounced_sync_push");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skillmgr_maybe_debounced_sync_push <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
