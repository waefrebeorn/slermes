/* gate: gateway_delivery_ledger_py__debug_rows — traced copy of gateway_delivery_ledger_debug_rows */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_delivery_ledger_debug_rows(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_delivery_ledger_debug_rows");
    int result = gateway_delivery_ledger_debug_rows(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_delivery_ledger_debug_rows");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_delivery_ledger_debug_rows <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
