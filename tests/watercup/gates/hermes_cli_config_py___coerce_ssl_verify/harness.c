/* gate: hermes_cli_config_py___coerce_ssl_verify — traced copy of config_coerce_ssl_verify */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int config_coerce_ssl_verify(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("config_coerce_ssl_verify");
    int result = config_coerce_ssl_verify(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_coerce_ssl_verify");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_coerce_ssl_verify <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
