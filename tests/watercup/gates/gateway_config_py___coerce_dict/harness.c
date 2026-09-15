/* gate: gateway_config_py___coerce_dict — traced copy of gateway_config_u_coerce_dict */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_config_u_coerce_dict(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_config_u_coerce_dict");
    int result = gateway_config_u_coerce_dict(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_config_u_coerce_dict");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_config_u_coerce_dict <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
