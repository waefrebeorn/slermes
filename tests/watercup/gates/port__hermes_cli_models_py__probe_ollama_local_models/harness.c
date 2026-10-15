/* gate: port__hermes_cli_models_py__probe_ollama_local_models — traced copy of models_probe_ollama_local_models (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * models_probe_ollama_local_models(const char *base_url, double timeout,
                                       const char *headers_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_probe_ollama_local_models <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_probe_ollama_local_models");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = models_probe_ollama_local_models((int)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_probe_ollama_local_models");
    }
    return 0;
}
