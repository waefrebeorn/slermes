/* gate: retry_utils_py__zai_coding_overload_retry_ceiling — traced copy of zai_coding_overload_retry_ceiling */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int zai_coding_overload_retry_ceiling(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("zai_coding_overload_retry_ceiling");
    int result = zai_coding_overload_retry_ceiling(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("zai_coding_overload_retry_ceiling");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s zai_coding_overload_retry_ceiling <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
