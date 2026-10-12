/* gate: port__hermes_cli_auth_py___save_model_choice — traced copy of auth_save_model_choice (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void auth_save_model_choice(const char *model_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_save_model_choice <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_save_model_choice");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    auth_save_model_choice(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_save_model_choice");
    }
    return 0;
}
