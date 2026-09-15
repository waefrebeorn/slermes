/* gate: agent_credential_pool_py___entry_needs_refresh — traced copy of cpl_entry_needs_refresh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cpl_entry_needs_refresh(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cpl_entry_needs_refresh");
    bool result = cpl_entry_needs_refresh(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cpl_entry_needs_refresh");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_entry_needs_refresh <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
