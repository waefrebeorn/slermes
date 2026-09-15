/* gate: gateway_shutdown_flush_py___fsync_directory — traced copy of shutdown_flush_fsync_directory */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shutdown_flush_fsync_directory(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("shutdown_flush_fsync_directory");
    int result = shutdown_flush_fsync_directory(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shutdown_flush_fsync_directory");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shutdown_flush_fsync_directory <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
