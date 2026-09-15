/* gate: agent_conversation_compression_py__is_compaction_progress_status — traced copy of is_compaction_progress_status */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_compaction_progress_status(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_compaction_progress_status");
    bool result = is_compaction_progress_status(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_compaction_progress_status");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_compaction_progress_status <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
