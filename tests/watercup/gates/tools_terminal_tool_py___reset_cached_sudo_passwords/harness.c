/* gate: tools_terminal_tool_py___reset_cached_sudo_passwords — traced copy of tt_u_reset_cached_sudo_passwords */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tt_u_reset_cached_sudo_passwords(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tt_u_reset_cached_sudo_passwords");
    int result = tt_u_reset_cached_sudo_passwords(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tt_u_reset_cached_sudo_passwords");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tt_u_reset_cached_sudo_passwords <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
