/* gate: hermes_cli_nous_account_py___coerce_bool — traced copy of nac_coerce_bool */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool nac_coerce_bool(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nac_coerce_bool");
    bool result = nac_coerce_bool(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nac_coerce_bool");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nac_coerce_bool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
