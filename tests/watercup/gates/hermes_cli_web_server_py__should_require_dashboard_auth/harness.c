/* gate: hermes_cli_web_server_py__should_require_dashboard_auth — traced copy of should_require_dashboard_auth */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int should_require_dashboard_auth(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("should_require_dashboard_auth");
    int result = should_require_dashboard_auth(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("should_require_dashboard_auth");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s should_require_dashboard_auth <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
