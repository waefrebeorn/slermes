/* gate: tools_computer_use_cua_backend_py___manifest_is_mode_independent — traced copy of cua_manifest_is_mode_independent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cua_manifest_is_mode_independent(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cua_manifest_is_mode_independent");
    bool result = cua_manifest_is_mode_independent(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cua_manifest_is_mode_independent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cua_manifest_is_mode_independent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
