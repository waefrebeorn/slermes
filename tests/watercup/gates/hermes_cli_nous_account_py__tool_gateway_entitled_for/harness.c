/* gate: hermes_cli_nous_account_py__tool_gateway_entitled_for — traced copy of nous_tool_gateway_entitled_for */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nous_tool_gateway_entitled_for(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nous_tool_gateway_entitled_for");
    int result = nous_tool_gateway_entitled_for(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nous_tool_gateway_entitled_for");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nous_tool_gateway_entitled_for <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
