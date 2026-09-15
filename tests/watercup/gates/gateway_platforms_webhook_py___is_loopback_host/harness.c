/* gate: gateway_platforms_webhook_py___is_loopback_host — traced copy of is_loopback_host */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_loopback_host(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_loopback_host");
    bool result = is_loopback_host(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_loopback_host");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_loopback_host <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
