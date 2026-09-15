/* gate: tools_approval_py__approve_permanent — traced copy of apr_approve_permanent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apr_approve_permanent(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("apr_approve_permanent");
    int result = apr_approve_permanent(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr_approve_permanent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr_approve_permanent <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
