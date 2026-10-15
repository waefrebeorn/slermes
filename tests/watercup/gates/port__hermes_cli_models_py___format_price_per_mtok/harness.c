/* gate: port__hermes_cli_models_py___format_price_per_mtok — traced copy of models_format_price_per_mtok (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * models_format_price_per_mtok(const char *per_token_str);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_format_price_per_mtok <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_format_price_per_mtok");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = models_format_price_per_mtok(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_format_price_per_mtok");
    }
    return 0;
}
