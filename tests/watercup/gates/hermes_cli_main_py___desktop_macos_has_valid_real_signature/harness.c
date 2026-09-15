/* gate: hermes_cli_main_py___desktop_macos_has_valid_real_signature — traced copy of main_g_desktop_macos_has_valid_real_signature */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool main_g_desktop_macos_has_valid_real_signature(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_g_desktop_macos_has_valid_real_signature");
    bool result = main_g_desktop_macos_has_valid_real_signature(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_g_desktop_macos_has_valid_real_signature");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_desktop_macos_has_valid_real_signature <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
