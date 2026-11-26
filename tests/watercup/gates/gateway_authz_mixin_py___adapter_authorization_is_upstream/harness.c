/* gate: gateway_authz_mixin_py___adapter_authorization_is_upstream — traced copy of authz_adapter_authorization_is_upstream */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int authz_adapter_authorization_is_upstream(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("authz_adapter_authorization_is_upstream");
    int result = authz_adapter_authorization_is_upstream(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("authz_adapter_authorization_is_upstream");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s authz_adapter_authorization_is_upstream <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
