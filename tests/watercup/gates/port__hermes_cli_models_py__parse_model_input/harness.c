/* gate: port__hermes_cli_models_py__parse_model_input — traced copy of model_parse_model_input (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void model_parse_model_input(const char *raw, const char *current_provider,
                             char *provider_out, size_t poutsz,
                             char *model_out, size_t moutsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_parse_model_input <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_parse_model_input");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    model_parse_model_input();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_parse_model_input");
    }
    return 0;
}
