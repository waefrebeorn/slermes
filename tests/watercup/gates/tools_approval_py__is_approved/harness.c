/* gate: tools_approval_py__is_approved — traced copy of apr_is_approved */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apr_is_approved(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apr_is_approved");
    int result = apr_is_approved(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr_is_approved");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr_is_approved <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
