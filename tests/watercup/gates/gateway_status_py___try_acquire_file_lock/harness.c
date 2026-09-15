/* gate: gateway_status_py___try_acquire_file_lock — traced copy of gwstatus_is_gateway_runtime_lock_active */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gwstatus_is_gateway_runtime_lock_active(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gwstatus_is_gateway_runtime_lock_active");
    bool result = gwstatus_is_gateway_runtime_lock_active(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gwstatus_is_gateway_runtime_lock_active");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwstatus_is_gateway_runtime_lock_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
