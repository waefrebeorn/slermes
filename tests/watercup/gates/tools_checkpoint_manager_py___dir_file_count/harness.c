/* gate: tools_checkpoint_manager_py___dir_file_count — traced copy of _dir_file_count */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _dir_file_count(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_dir_file_count");
    int result = _dir_file_count(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_dir_file_count");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _dir_file_count <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
