/* gate: port__hermes_cli_models_py___ai_gateway_model_is_free — traced copy of _ai_gateway_model_is_free (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool _ai_gateway_model_is_free(const char *pricing_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _ai_gateway_model_is_free <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_ai_gateway_model_is_free");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = _ai_gateway_model_is_free(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_ai_gateway_model_is_free");
    }
    return 0;
}
