/* gate: port__hermes_cli_update_cmd_py___looks_like_desktop_control_plane — traced copy of _looks_like_desktop_control_plane (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _looks_like_desktop_control_plane(const char *cmdline);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _looks_like_desktop_control_plane <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_looks_like_desktop_control_plane");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _looks_like_desktop_control_plane(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_looks_like_desktop_control_plane");
    }
    return 0;
}
