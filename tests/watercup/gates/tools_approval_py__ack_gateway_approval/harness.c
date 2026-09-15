/* gate: tools_approval_py__ack_gateway_approval — traced copy of approval_ack_gateway_approval */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int approval_ack_gateway_approval(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("approval_ack_gateway_approval");
    int result = approval_ack_gateway_approval(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("approval_ack_gateway_approval");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s approval_ack_gateway_approval <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
