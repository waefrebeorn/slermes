/* gate: agent_copilot_acp_client_py___ensure_path_within_cwd — traced copy of cac_ensure_path_within_cwd */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cac_ensure_path_within_cwd(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cac_ensure_path_within_cwd");
    int result = cac_ensure_path_within_cwd(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cac_ensure_path_within_cwd");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cac_ensure_path_within_cwd <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
