/* gate: agent_verification_evidence_py__mark_workspace_edited — traced copy of vev_mark_workspace_edited */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int vev_mark_workspace_edited(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("vev_mark_workspace_edited");
    int result = vev_mark_workspace_edited(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("vev_mark_workspace_edited");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vev_mark_workspace_edited <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
