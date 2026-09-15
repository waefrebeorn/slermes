/* gate: tools_ansi_strip_py__sanitize_display_text — traced copy of tools_ansi_strip_sanitize_display_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_ansi_strip_sanitize_display_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_ansi_strip_sanitize_display_text");
    int result = tools_ansi_strip_sanitize_display_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_ansi_strip_sanitize_display_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_ansi_strip_sanitize_display_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
