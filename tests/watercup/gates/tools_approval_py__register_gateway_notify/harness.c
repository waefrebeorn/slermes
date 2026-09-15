/* gate: tools_approval_py__register_gateway_notify — traced copy of apr2_register_gateway_notify */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apr2_register_gateway_notify(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("apr2_register_gateway_notify");
    int result = apr2_register_gateway_notify(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr2_register_gateway_notify");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_register_gateway_notify <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
