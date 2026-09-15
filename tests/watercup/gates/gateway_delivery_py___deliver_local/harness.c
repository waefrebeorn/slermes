/* gate: gateway_delivery_py___deliver_local — traced copy of cli_gateway_delivery__deliver_local */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_gateway_delivery__deliver_local(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_gateway_delivery__deliver_local");
    int result = cli_gateway_delivery__deliver_local(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_gateway_delivery__deliver_local");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_delivery__deliver_local <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
