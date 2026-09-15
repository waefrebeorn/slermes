/* gate: tools_async_delegation_py___new_delegation_id — traced copy of adel_u_new_delegation_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int adel_u_new_delegation_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("adel_u_new_delegation_id");
    int result = adel_u_new_delegation_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("adel_u_new_delegation_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s adel_u_new_delegation_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
