/* gate: tools_file_tools_py___record_not_found — traced copy of filetools_record_not_found */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int filetools_record_not_found(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("filetools_record_not_found");
    int result = filetools_record_not_found(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("filetools_record_not_found");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s filetools_record_not_found <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
