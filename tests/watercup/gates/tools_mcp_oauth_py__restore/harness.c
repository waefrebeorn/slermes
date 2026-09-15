/* gate: tools_mcp_oauth_py__restore — traced copy of mco_restore */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mco_restore(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mco_restore");
    int result = mco_restore(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mco_restore");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mco_restore <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
