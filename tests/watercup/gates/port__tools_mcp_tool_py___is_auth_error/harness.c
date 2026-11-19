/* gate: port__tools_mcp_tool_py___is_auth_error — traced copy of mct_is_auth_error (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool mct_is_auth_error(long status_code, const char *msg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_is_auth_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mct_is_auth_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = mct_is_auth_error((long)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mct_is_auth_error");
    }
    return 0;
}
