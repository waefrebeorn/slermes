/* gate: port__hermes_cli_models_py___openrouter_model_supports_tools — traced copy of openrouter_model_supports_tools (bool_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool openrouter_model_supports_tools(const char *supported_params, int has_list);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s openrouter_model_supports_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("openrouter_model_supports_tools");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = openrouter_model_supports_tools(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("openrouter_model_supports_tools");
    }
    return 0;
}
