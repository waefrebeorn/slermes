/* gate: tools_file_operations_py___probe_write_target — traced copy of _probe_write_target */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _probe_write_target(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_probe_write_target");
    int result = _probe_write_target(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_probe_write_target");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _probe_write_target <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
