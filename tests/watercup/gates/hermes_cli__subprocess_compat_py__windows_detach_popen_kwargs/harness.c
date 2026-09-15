/* gate: hermes_cli__subprocess_compat_py__windows_detach_popen_kwargs — traced copy of hermes_cli__subprocess_compat_windows_detach_popen_kwargs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli__subprocess_compat_windows_detach_popen_kwargs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli__subprocess_compat_windows_detach_popen_kwargs");
    int result = hermes_cli__subprocess_compat_windows_detach_popen_kwargs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli__subprocess_compat_windows_detach_popen_kwargs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli__subprocess_compat_windows_detach_popen_kwargs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
