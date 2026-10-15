/* gate: port__hermes_cli_web_routers_local_models_py__local_models_runtime_install — traced copy of local_models_runtime_install (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * local_models_runtime_install(const char *tag);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s local_models_runtime_install <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("local_models_runtime_install");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = local_models_runtime_install(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("local_models_runtime_install");
    }
    return 0;
}
