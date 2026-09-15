/* gate: agent_transports_codex_app_server_session_py___apply_compaction_notification — traced copy of cas2_apply_compaction_notification */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cas2_apply_compaction_notification(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cas2_apply_compaction_notification");
    int result = cas2_apply_compaction_notification(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cas2_apply_compaction_notification");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cas2_apply_compaction_notification <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
