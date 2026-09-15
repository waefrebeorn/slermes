/* gate: gateway_authz_mixin_py___adapter_authorization_is_upstream — traced copy of authz_adapter_authorization_is_upstream */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int authz_adapter_authorization_is_upstream(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("authz_adapter_authorization_is_upstream");
    int result = authz_adapter_authorization_is_upstream(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("authz_adapter_authorization_is_upstream");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s authz_adapter_authorization_is_upstream <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
