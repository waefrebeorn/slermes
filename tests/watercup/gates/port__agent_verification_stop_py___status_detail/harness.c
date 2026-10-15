/* gate: port__agent_verification_stop_py___status_detail — traced copy of verification_stop_status_detail (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * verification_stop_status_detail(const char *status_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s verification_stop_status_detail <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("verification_stop_status_detail");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = verification_stop_status_detail(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("verification_stop_status_detail");
    }
    return 0;
}
