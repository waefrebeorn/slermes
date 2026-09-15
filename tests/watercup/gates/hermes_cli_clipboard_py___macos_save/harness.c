/* gate: hermes_cli_clipboard_py___macos_save — traced copy of clipboard_macos_save */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int clipboard_macos_save(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clipboard_macos_save");
    int result = clipboard_macos_save(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clipboard_macos_save");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clipboard_macos_save <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
