/* gate: tools_environments_modal_py___ensure_modal_sdk — traced copy of tools_environments_modal_u_ensure_modal_sdk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_environments_modal_u_ensure_modal_sdk(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_environments_modal_u_ensure_modal_sdk");
    int result = tools_environments_modal_u_ensure_modal_sdk(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_environments_modal_u_ensure_modal_sdk");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_environments_modal_u_ensure_modal_sdk <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
