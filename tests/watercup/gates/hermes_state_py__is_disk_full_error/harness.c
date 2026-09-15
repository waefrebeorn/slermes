/* gate: hermes_state_py__is_disk_full_error — traced copy of hs_is_disk_full_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hs_is_disk_full_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hs_is_disk_full_error");
    bool result = hs_is_disk_full_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hs_is_disk_full_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hs_is_disk_full_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
