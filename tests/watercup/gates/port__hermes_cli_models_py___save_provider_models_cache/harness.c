/* gate: port__hermes_cli_models_py___save_provider_models_cache — traced copy of model_save_provider_models_cache (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void model_save_provider_models_cache(const char *json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_save_provider_models_cache <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_save_provider_models_cache");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    model_save_provider_models_cache();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_save_provider_models_cache");
    }
    return 0;
}
