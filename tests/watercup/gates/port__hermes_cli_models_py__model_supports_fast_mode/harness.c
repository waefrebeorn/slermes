/* gate: port__hermes_cli_models_py__model_supports_fast_mode — traced copy of model_supports_fast_mode (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int model_supports_fast_mode(const char *model_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_supports_fast_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_supports_fast_mode");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = model_supports_fast_mode(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_supports_fast_mode");
    }
    return 0;
}
