/* gate: hermes_cli_doctor_py___has_provider_env_config — traced copy of doctor_has_provider_env_config */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int doctor_has_provider_env_config(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("doctor_has_provider_env_config");
    int result = doctor_has_provider_env_config(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("doctor_has_provider_env_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s doctor_has_provider_env_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
