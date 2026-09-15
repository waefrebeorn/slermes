/* gate: hermes_cli_backup_py___in_excluded_root_dir — traced copy of _in_excluded_root_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _in_excluded_root_dir(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_in_excluded_root_dir");
    int result = _in_excluded_root_dir(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_in_excluded_root_dir");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _in_excluded_root_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
