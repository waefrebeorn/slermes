/* gate: hermes_cli_web_server_py___maybe_open_browser — traced copy of web_maybe_open_browser */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool web_maybe_open_browser(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("web_maybe_open_browser");
    bool result = web_maybe_open_browser(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("web_maybe_open_browser");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s web_maybe_open_browser <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
