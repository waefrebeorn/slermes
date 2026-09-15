/* gate: gateway_status_py__looks_like_gateway_command_line — traced copy of gwstatus_looks_like_gateway_command_line */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gwstatus_looks_like_gateway_command_line(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gwstatus_looks_like_gateway_command_line");
    bool result = gwstatus_looks_like_gateway_command_line(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gwstatus_looks_like_gateway_command_line");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwstatus_looks_like_gateway_command_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
