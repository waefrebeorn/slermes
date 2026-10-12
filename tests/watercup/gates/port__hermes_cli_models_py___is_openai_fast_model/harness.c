/* gate: port__hermes_cli_models_py___is_openai_fast_model — traced copy of models_is_openai_fast_model (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int models_is_openai_fast_model(const char *model_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_is_openai_fast_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("models_is_openai_fast_model");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = models_is_openai_fast_model(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_is_openai_fast_model");
    }
    return 0;
}
