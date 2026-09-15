/* gate: gateway_platforms_webhook_py__on_processing_complete — traced copy of webhook_u_on_processing_complete */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int webhook_u_on_processing_complete(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("webhook_u_on_processing_complete");
    int result = webhook_u_on_processing_complete(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("webhook_u_on_processing_complete");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s webhook_u_on_processing_complete <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
