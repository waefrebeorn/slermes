/* gate: gateway_platforms_webhook_py___hmac_str_equal — traced copy of webhook_hmac_str_equal */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int webhook_hmac_str_equal(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("webhook_hmac_str_equal");
    int result = webhook_hmac_str_equal(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("webhook_hmac_str_equal");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s webhook_hmac_str_equal <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
