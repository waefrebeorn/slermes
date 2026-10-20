/* gate: port__hermes_cli_models_py__detect_static_provider_for_model — traced copy of model_detect_static_provider_for_model (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int model_detect_static_provider_for_model(const char *model_name,
                                           const char *current_provider,
                                           char *provider_out, size_t poutsz,
                                           char *model_out, size_t moutsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_detect_static_provider_for_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_detect_static_provider_for_model");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = model_detect_static_provider_for_model(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_detect_static_provider_for_model");
    }
    return 0;
}
