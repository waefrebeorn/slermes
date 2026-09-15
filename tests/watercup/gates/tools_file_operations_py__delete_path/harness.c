/* gate: tools_file_operations_py__delete_path — traced copy of file_fs_ops_delete_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool file_fs_ops_delete_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("file_fs_ops_delete_path");
    bool result = file_fs_ops_delete_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_fs_ops_delete_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_fs_ops_delete_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
