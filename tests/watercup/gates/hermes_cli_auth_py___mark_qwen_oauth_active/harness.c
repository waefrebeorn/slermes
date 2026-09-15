/* gate: hermes_cli_auth_py___mark_qwen_oauth_active — traced copy of auth_u_mark_qwen_oauth_active */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_u_mark_qwen_oauth_active(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_u_mark_qwen_oauth_active");
    int result = auth_u_mark_qwen_oauth_active(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_u_mark_qwen_oauth_active");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_u_mark_qwen_oauth_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
