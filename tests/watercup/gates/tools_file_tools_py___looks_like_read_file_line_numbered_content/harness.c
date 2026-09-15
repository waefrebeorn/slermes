/* gate: tools_file_tools_py___looks_like_read_file_line_numbered_content — traced copy of file_tools_looks_like_read_file_line_numbered_content */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int file_tools_looks_like_read_file_line_numbered_content(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("file_tools_looks_like_read_file_line_numbered_content");
    int result = file_tools_looks_like_read_file_line_numbered_content(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_tools_looks_like_read_file_line_numbered_content");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_tools_looks_like_read_file_line_numbered_content <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
