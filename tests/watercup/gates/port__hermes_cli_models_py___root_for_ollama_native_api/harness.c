/* gate: port__hermes_cli_models_py___root_for_ollama_native_api — traced copy of models_root_for_ollama_native_api (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * models_root_for_ollama_native_api(const char *base_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_root_for_ollama_native_api <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_root_for_ollama_native_api");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = models_root_for_ollama_native_api(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_root_for_ollama_native_api");
    }
    return 0;
}
