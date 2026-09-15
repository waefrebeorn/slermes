/* gate: tools_approval_py___record_denial — traced copy of apr2_record_denial */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apr2_record_denial(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apr2_record_denial");
    int result = apr2_record_denial(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr2_record_denial");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_record_denial <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
