/* gate: tools_terminal_tool_py__set_sudo_password_callback — traced copy of ttm_set_sudo_password_callback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ttm_set_sudo_password_callback(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ttm_set_sudo_password_callback");
    int result = ttm_set_sudo_password_callback(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ttm_set_sudo_password_callback");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ttm_set_sudo_password_callback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
