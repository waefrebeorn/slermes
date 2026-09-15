/* gate: tools_read_extract_py__is_extractable_document — traced copy of read_extract_is_extractable_document */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool read_extract_is_extractable_document(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("read_extract_is_extractable_document");
    bool result = read_extract_is_extractable_document(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("read_extract_is_extractable_document");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s read_extract_is_extractable_document <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
