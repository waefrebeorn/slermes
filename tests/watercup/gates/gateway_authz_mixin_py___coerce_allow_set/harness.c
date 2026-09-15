/* gate: gateway_authz_mixin_py___coerce_allow_set — traced copy of gateway_authz_mixin_u_coerce_allow_set */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_authz_mixin_u_coerce_allow_set(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_authz_mixin_u_coerce_allow_set");
    int result = gateway_authz_mixin_u_coerce_allow_set(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_authz_mixin_u_coerce_allow_set");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_authz_mixin_u_coerce_allow_set <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
