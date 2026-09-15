/* gate: tools_url_safety_py___is_blocked_ip — traced copy of cli_tools_url_safety__is_blocked_ip */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_tools_url_safety__is_blocked_ip(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_tools_url_safety__is_blocked_ip");
    int result = cli_tools_url_safety__is_blocked_ip(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_tools_url_safety__is_blocked_ip");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_url_safety__is_blocked_ip <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
