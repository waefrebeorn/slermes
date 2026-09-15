/* gate: hermes_cli_clipboard_py___convert_to_png — traced copy of clipboard_convert_to_png */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int clipboard_convert_to_png(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clipboard_convert_to_png");
    int result = clipboard_convert_to_png(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clipboard_convert_to_png");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clipboard_convert_to_png <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
