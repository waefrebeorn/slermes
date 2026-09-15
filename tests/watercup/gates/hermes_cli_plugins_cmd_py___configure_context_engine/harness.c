/* gate: hermes_cli_plugins_cmd_py___configure_context_engine — traced copy of pcmd_configure_context_engine */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pcmd_configure_context_engine(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("pcmd_configure_context_engine");
    int result = pcmd_configure_context_engine(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pcmd_configure_context_engine");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pcmd_configure_context_engine <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
