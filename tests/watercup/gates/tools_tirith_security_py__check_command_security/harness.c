/* gate: tools_tirith_security_py__check_command_security — traced copy of _is_app_tld_finding */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _is_app_tld_finding(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_app_tld_finding");
    bool result = _is_app_tld_finding(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_app_tld_finding");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_app_tld_finding <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
