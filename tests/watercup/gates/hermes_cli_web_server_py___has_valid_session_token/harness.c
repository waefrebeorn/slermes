/* gate: hermes_cli_web_server_py___has_valid_session_token — traced copy of ws_has_valid_session_token */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ws_has_valid_session_token(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_has_valid_session_token");
    bool result = ws_has_valid_session_token(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_has_valid_session_token");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_has_valid_session_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
