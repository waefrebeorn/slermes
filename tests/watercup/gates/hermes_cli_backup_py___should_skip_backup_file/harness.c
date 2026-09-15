/* gate: hermes_cli_backup_py___should_skip_backup_file — traced copy of _should_skip_backup_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _should_skip_backup_file(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_should_skip_backup_file");
    int result = _should_skip_backup_file(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_should_skip_backup_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _should_skip_backup_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
