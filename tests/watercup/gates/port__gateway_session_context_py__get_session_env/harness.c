/* gate: port__gateway_session_context_py__get_session_env — traced copy of sct_get_session_env (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * sct_get_session_env(const char *legacy_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sct_get_session_env <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sct_get_session_env");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = sct_get_session_env(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sct_get_session_env");
    }
    return 0;
}
