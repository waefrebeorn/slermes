/* gate: gateway_platforms_msgraph_webhook_py___verify_client_state — traced copy of msgraph_verify_client_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msgraph_verify_client_state(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msgraph_verify_client_state");
    int result = msgraph_verify_client_state(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msgraph_verify_client_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msgraph_verify_client_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
