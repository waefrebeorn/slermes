/* gate: port__tools_mcp_tool_py___get_auth_error_types — traced copy of mct_get_auth_error_types (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * mct_get_auth_error_types(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_get_auth_error_types <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mct_get_auth_error_types");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = mct_get_auth_error_types();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mct_get_auth_error_types");
    }
    return 0;
}
