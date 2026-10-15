/* gate: port__hermes_cli_web_server_py___normalize_main_model_assignment — traced copy of normalize_main_model_assignment (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * normalize_main_model_assignment(const char *provider, const char *model);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_main_model_assignment <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("normalize_main_model_assignment");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = normalize_main_model_assignment(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_main_model_assignment");
    }
    return 0;
}
