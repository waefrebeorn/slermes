/* gate: port__hermes_cli_runtime_provider_py__canonical_custom_identity — traced copy of canonical_custom_identity (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * canonical_custom_identity(const char *base_url, const char *config_provider);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s canonical_custom_identity <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("canonical_custom_identity");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = canonical_custom_identity(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("canonical_custom_identity");
    }
    return 0;
}
