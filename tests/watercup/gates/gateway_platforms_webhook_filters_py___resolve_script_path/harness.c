/* gate: gateway_platforms_webhook_filters_py___resolve_script_path — traced copy of gateway_platforms_webhook_filt_u_resolve_script_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_platforms_webhook_filt_u_resolve_script_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_platforms_webhook_filt_u_resolve_script_path");
    int result = gateway_platforms_webhook_filt_u_resolve_script_path(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_platforms_webhook_filt_u_resolve_script_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_platforms_webhook_filt_u_resolve_script_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
