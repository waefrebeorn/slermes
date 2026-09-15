/* gate: tools_registry_py___check_fn_cached — traced copy of tools_registry_u_check_fn_cached */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_registry_u_check_fn_cached(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_registry_u_check_fn_cached");
    int result = tools_registry_u_check_fn_cached(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_registry_u_check_fn_cached");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_registry_u_check_fn_cached <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
