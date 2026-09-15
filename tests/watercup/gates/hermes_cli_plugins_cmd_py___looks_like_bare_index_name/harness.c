/* gate: hermes_cli_plugins_cmd_py___looks_like_bare_index_name — traced copy of _looks_like_bare_index_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _looks_like_bare_index_name(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_looks_like_bare_index_name");
    bool result = _looks_like_bare_index_name(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_looks_like_bare_index_name");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _looks_like_bare_index_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
