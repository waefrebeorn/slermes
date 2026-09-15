/* gate: gateway_platforms_webhook_py___is_webhook_silence_response — traced copy of wh_is_webhook_silence_response */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool wh_is_webhook_silence_response(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wh_is_webhook_silence_response");
    bool result = wh_is_webhook_silence_response(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wh_is_webhook_silence_response");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wh_is_webhook_silence_response <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
