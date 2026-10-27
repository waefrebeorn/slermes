/* gate: port__hermes_cli_web_server_py___parent_start_markers_match — traced copy of ws_parent_start_markers_match (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool ws_parent_start_markers_match(const char *actual, const char *expected);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_parent_start_markers_match <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_parent_start_markers_match");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = ws_parent_start_markers_match(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_parent_start_markers_match");
    }
    return 0;
}
