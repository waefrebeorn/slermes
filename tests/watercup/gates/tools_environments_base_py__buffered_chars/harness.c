/* gate: tools_environments_base_py__buffered_chars — traced copy of envb_buffered_chars */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int envb_buffered_chars(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("envb_buffered_chars");
    int result = envb_buffered_chars(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("envb_buffered_chars");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_buffered_chars <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
