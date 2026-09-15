/* gate: tools_file_tools_py___search_result_read_block_error — traced copy of file_tools_search_result_read_block_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool file_tools_search_result_read_block_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("file_tools_search_result_read_block_error");
    bool result = file_tools_search_result_read_block_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_tools_search_result_read_block_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_tools_search_result_read_block_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
