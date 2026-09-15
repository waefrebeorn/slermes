/* gate: hermes_cli_nous_account_py___paid_service_access_from_payload — traced copy of nous_u_paid_service_access_from_payload */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nous_u_paid_service_access_from_payload(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nous_u_paid_service_access_from_payload");
    int result = nous_u_paid_service_access_from_payload(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nous_u_paid_service_access_from_payload");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nous_u_paid_service_access_from_payload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
