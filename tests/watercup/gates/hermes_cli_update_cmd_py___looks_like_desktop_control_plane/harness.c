/* gate: hermes_cli_update_cmd_py___looks_like_desktop_control_plane — traced copy of _looks_like_desktop_control_plane */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _looks_like_desktop_control_plane(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_looks_like_desktop_control_plane");
    int result = _looks_like_desktop_control_plane(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_looks_like_desktop_control_plane");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _looks_like_desktop_control_plane <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
