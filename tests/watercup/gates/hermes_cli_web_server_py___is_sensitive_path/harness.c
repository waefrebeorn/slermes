/* gate: hermes_cli_web_server_py___is_sensitive_path — traced copy of is_sensitive_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_sensitive_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_sensitive_path");
    bool result = is_sensitive_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_sensitive_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_sensitive_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
