/* gate: hermes_cli_doctor_py___has_healthy_oauth_fallback_for_apikey_provider — traced copy of doctor_has_healthy_oauth_fallback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool doctor_has_healthy_oauth_fallback(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("doctor_has_healthy_oauth_fallback");
    bool result = doctor_has_healthy_oauth_fallback(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("doctor_has_healthy_oauth_fallback");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s doctor_has_healthy_oauth_fallback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
