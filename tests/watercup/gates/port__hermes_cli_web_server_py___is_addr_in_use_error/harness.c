/* gate: port__hermes_cli_web_server_py___is_addr_in_use_error — traced copy of ws_is_addr_in_use_error (bool_int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool ws_is_addr_in_use_error(int errnum);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_is_addr_in_use_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_is_addr_in_use_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = ws_is_addr_in_use_error((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_is_addr_in_use_error");
    }
    return 0;
}
