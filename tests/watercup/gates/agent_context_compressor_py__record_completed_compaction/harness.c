/* gate: agent_context_compressor_py__record_completed_compaction — traced copy of ctxc_record_completed_compaction */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ctxc_record_completed_compaction(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ctxc_record_completed_compaction");
    int result = ctxc_record_completed_compaction(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ctxc_record_completed_compaction");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ctxc_record_completed_compaction <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
