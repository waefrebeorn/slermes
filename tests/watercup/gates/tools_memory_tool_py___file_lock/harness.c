/* gate: tools_memory_tool_py___file_lock — traced copy of memory_store_file_lock */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int memory_store_file_lock(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("memory_store_file_lock");
    int result = memory_store_file_lock(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("memory_store_file_lock");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s memory_store_file_lock <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
