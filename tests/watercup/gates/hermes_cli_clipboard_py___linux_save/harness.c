/* gate: hermes_cli_clipboard_py___linux_save — traced copy of clipboard_linux_save */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int clipboard_linux_save(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clipboard_linux_save");
    int result = clipboard_linux_save(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clipboard_linux_save");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clipboard_linux_save <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
