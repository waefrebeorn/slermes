/* gate: gateway_wake_py__adapter_supports_push — traced copy of gateway_wake_adapter_supports_push */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_wake_adapter_supports_push(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_wake_adapter_supports_push");
    int result = gateway_wake_adapter_supports_push(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_wake_adapter_supports_push");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_wake_adapter_supports_push <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
