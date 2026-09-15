/* gate: gateway_platforms_msgraph_webhook_py___handle_validation — traced copy of msgraph_handle_validation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msgraph_handle_validation(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("msgraph_handle_validation");
    int result = msgraph_handle_validation(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msgraph_handle_validation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msgraph_handle_validation <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
