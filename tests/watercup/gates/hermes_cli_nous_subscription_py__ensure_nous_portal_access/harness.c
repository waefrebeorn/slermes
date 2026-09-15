/* gate: hermes_cli_nous_subscription_py__ensure_nous_portal_access — traced copy of nsub_ensure_nous_portal_access */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nsub_ensure_nous_portal_access(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nsub_ensure_nous_portal_access");
    int result = nsub_ensure_nous_portal_access(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nsub_ensure_nous_portal_access");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nsub_ensure_nous_portal_access <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
