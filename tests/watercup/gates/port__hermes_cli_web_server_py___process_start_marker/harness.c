/* gate: port__hermes_cli_web_server_py___process_start_marker — traced copy of ws_process_start_marker (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool ws_process_start_marker(int pid, char *out, size_t sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_process_start_marker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_process_start_marker");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = ws_process_start_marker((int)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 5], (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_process_start_marker");
    }
    return 0;
}
