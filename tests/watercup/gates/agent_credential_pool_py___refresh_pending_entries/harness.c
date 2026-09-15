/* gate: agent_credential_pool_py___refresh_pending_entries — traced copy of cp_refresh_pending_entries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cp_refresh_pending_entries(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cp_refresh_pending_entries");
    int result = cp_refresh_pending_entries(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cp_refresh_pending_entries");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cp_refresh_pending_entries <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
