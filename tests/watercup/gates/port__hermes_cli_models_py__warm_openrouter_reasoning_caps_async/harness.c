/* gate: port__hermes_cli_models_py__warm_openrouter_reasoning_caps_async — traced copy of models_warm_openrouter_reasoning_caps_async (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void models_warm_openrouter_reasoning_caps_async(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_warm_openrouter_reasoning_caps_async <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_warm_openrouter_reasoning_caps_async");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    models_warm_openrouter_reasoning_caps_async();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_warm_openrouter_reasoning_caps_async");
    }
    return 0;
}
