/* gate: port__hermes_cli_models_py___ollama_probe_cache_key — traced copy of models_ollama_probe_cache_key (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * models_ollama_probe_cache_key(const char *root, const char *headers_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_ollama_probe_cache_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_ollama_probe_cache_key");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = models_ollama_probe_cache_key(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_ollama_probe_cache_key");
    }
    return 0;
}
