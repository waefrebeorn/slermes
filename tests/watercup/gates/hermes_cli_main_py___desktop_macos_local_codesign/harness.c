/* gate: hermes_cli_main_py___desktop_macos_local_codesign — traced copy of main_g_desktop_macos_local_codesign */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_g_desktop_macos_local_codesign(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("main_g_desktop_macos_local_codesign");
    int result = main_g_desktop_macos_local_codesign(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_g_desktop_macos_local_codesign");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_desktop_macos_local_codesign <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
