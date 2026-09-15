/* gate: hermes_cli_dashboard_auth_cookies_py__detect_https — traced copy of dash_detect_https */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int dash_detect_https(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("dash_detect_https");
    int result = dash_detect_https(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("dash_detect_https");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dash_detect_https <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
