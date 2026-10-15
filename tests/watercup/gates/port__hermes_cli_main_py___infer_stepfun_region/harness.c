/* gate: port__hermes_cli_main_py___infer_stepfun_region — traced copy of infer_stepfun_region (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * infer_stepfun_region(const char *base_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s infer_stepfun_region <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("infer_stepfun_region");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = infer_stepfun_region(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("infer_stepfun_region");
    }
    return 0;
}
