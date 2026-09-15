/* gate: tools_approval_py__submit_pending — traced copy of approval_submit_pending */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int approval_submit_pending(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("approval_submit_pending");
    int result = approval_submit_pending(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("approval_submit_pending");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s approval_submit_pending <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
