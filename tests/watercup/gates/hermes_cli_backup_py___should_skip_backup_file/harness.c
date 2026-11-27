/* gate: {'name': 'hermes_cli_backup_py___should_skip_backup_file', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_should_skip_backup_file', 'py_module': 'hermes_cli/backup.py', 'py_fn': '_should_skip_backup_file', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _should_skip_backup_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_should_skip_backup_file";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _should_skip_backup_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_should_skip_backup_file");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_should_skip_backup_file");
    }
    return 0;
}
