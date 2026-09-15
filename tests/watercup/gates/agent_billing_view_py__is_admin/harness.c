/* gate: agent_billing_view_py__is_admin — traced copy of biv_is_admin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool biv_is_admin(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("biv_is_admin");
    bool result = biv_is_admin(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("biv_is_admin");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s biv_is_admin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
