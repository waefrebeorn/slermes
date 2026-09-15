/* gate: hermes_cli_clipboard_py___write_base64_image — traced copy of clipboard_write_base64_image */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int clipboard_write_base64_image(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("clipboard_write_base64_image");
    int result = clipboard_write_base64_image(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("clipboard_write_base64_image");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s clipboard_write_base64_image <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
