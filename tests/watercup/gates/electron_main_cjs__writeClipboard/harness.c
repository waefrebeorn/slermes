/* gate: electron_main_cjs__writeClipboard — traced copy of clipboard_write_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool clipboard_write_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clipboard_write_text");
    bool result = clipboard_write_text(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clipboard_write_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clipboard_write_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
