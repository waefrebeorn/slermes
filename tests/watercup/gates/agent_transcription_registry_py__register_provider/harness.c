/* gate: agent_transcription_registry_py__register_provider — traced copy of tcr_register_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tcr_register_provider(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("tcr_register_provider");
    int result = tcr_register_provider(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tcr_register_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tcr_register_provider <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
