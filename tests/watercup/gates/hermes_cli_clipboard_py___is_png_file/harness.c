/* gate: hermes_cli_clipboard_py___is_png_file — traced copy of clipboard_is_png_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool clipboard_is_png_file(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clipboard_is_png_file");
    bool result = clipboard_is_png_file(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clipboard_is_png_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clipboard_is_png_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
