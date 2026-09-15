/* gate: agent_auxiliary_client_py___client_declares — traced copy of _client_declares */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _client_declares(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_client_declares");
    int result = _client_declares(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_client_declares");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _client_declares <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
