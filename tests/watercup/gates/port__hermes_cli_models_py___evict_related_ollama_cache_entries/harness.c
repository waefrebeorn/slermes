/* gate: port__hermes_cli_models_py___evict_related_ollama_cache_entries — traced copy of models_evict_related_ollama_cache_entries (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void models_evict_related_ollama_cache_entries(const char *key);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_evict_related_ollama_cache_entries <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_evict_related_ollama_cache_entries");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    models_evict_related_ollama_cache_entries();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_evict_related_ollama_cache_entries");
    }
    return 0;
}
