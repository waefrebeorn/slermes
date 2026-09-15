/* gate: gateway_platforms_msgraph_webhook_py___resource_accepted — traced copy of msgraph_resource_accepted */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msgraph_resource_accepted(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msgraph_resource_accepted");
    int result = msgraph_resource_accepted(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msgraph_resource_accepted");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msgraph_resource_accepted <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
