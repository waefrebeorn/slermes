/* gate: hermes_cli__subprocess_compat_py__windows_detach_flags_without_breakaway — traced copy of hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay");
    int result = hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
