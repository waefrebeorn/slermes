/* gate: gateway_delivery_ledger_py__compute_obligation_id — traced copy of gateway_delivery_ledger_compute_obligation_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_delivery_ledger_compute_obligation_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_delivery_ledger_compute_obligation_id");
    int result = gateway_delivery_ledger_compute_obligation_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_delivery_ledger_compute_obligation_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_delivery_ledger_compute_obligation_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
