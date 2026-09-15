/* gate: hermes_cli_backup_py___should_exclude — traced copy of backup_should_exclude */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int backup_should_exclude(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("backup_should_exclude");
    int result = backup_should_exclude(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("backup_should_exclude");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s backup_should_exclude <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
