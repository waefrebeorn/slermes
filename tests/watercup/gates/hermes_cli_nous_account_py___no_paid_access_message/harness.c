/* gate: hermes_cli_nous_account_py___no_paid_access_message — traced copy of nous_u_no_paid_access_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nous_u_no_paid_access_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nous_u_no_paid_access_message");
    int result = nous_u_no_paid_access_message(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nous_u_no_paid_access_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nous_u_no_paid_access_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
