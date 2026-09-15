/* gate: tools_read_extract_py___col_index — traced copy of read_extract_col_index */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int read_extract_col_index(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("read_extract_col_index");
    int result = read_extract_col_index(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("read_extract_col_index");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s read_extract_col_index <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
