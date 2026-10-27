/* gate: port__hermes_cli_models_py__should_use_ollama_native_catalog — traced copy of models_should_use_ollama_native_catalog (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int models_should_use_ollama_native_catalog(const char *provider,
                                            const char *base_url,
                                            const char *headers_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_should_use_ollama_native_catalog <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_should_use_ollama_native_catalog");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = models_should_use_ollama_native_catalog(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_should_use_ollama_native_catalog");
    }
    return 0;
}
