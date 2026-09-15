/* gate: hermes_cli_models_py___same_ollama_native_root — traced copy of models_same_ollama_native_root */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int models_same_ollama_native_root(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("models_same_ollama_native_root");
    int result = models_same_ollama_native_root(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("models_same_ollama_native_root");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_same_ollama_native_root <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
