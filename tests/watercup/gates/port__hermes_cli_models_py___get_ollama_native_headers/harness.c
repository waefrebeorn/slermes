/* gate: port__hermes_cli_models_py___get_ollama_native_headers — traced copy of models_get_ollama_native_headers (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * models_get_ollama_native_headers(const char *base_url, const char *api_key);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_get_ollama_native_headers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_get_ollama_native_headers");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = models_get_ollama_native_headers(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_get_ollama_native_headers");
    }
    return 0;
}
