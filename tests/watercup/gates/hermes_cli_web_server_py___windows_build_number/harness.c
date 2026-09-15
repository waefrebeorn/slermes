/* gate: hermes_cli_web_server_py___windows_build_number — traced copy of web_windows_build_number */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int web_windows_build_number(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("web_windows_build_number");
    int result = web_windows_build_number(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("web_windows_build_number");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s web_windows_build_number <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
