/* gate: port__hermes_cli_models_py___credential_fingerprint — traced copy of model_credential_fingerprint (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * model_credential_fingerprint(const char *provider);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_credential_fingerprint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_credential_fingerprint");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = model_credential_fingerprint(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_credential_fingerprint");
    }
    return 0;
}
