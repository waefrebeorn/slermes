/* gate: hermes_cli_clipboard_py__is_remote_shell_session — traced copy of clipboard_is_remote_shell_session */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int clipboard_is_remote_shell_session(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clipboard_is_remote_shell_session");
    int result = clipboard_is_remote_shell_session(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clipboard_is_remote_shell_session");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clipboard_is_remote_shell_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
