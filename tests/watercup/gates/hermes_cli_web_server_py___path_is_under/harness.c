/* gate: hermes_cli_web_server_py___path_is_under — traced copy of ws_path_is_under */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_path_is_under(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_path_is_under");
    int result = ws_path_is_under(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_path_is_under");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_path_is_under <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
