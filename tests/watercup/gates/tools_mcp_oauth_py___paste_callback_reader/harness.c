/* gate: tools_mcp_oauth_py___paste_callback_reader — traced copy of mcpo_u_paste_callback_reader */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_u_paste_callback_reader(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_u_paste_callback_reader");
    int result = mcpo_u_paste_callback_reader(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_u_paste_callback_reader");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_u_paste_callback_reader <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
