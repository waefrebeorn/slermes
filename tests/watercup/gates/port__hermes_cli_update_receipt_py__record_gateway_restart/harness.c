/* gate: port__hermes_cli_update_receipt_py__record_gateway_restart — traced copy of record_gateway_restart */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern int record_gateway_restart(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("record_gateway_restart");
    int result = record_gateway_restart(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("record_gateway_restart");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s record_gateway_restart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
