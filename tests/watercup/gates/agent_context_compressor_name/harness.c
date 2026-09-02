/* gate: agent_context_compressor_name — traces cc_name() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include <json.h>

char *cc_name(void);

static void run_one(void) {
    WCUP_BEGIN("agent_context_compressor_name");
    WCUP_BRANCH(1);
    const char *n = cc_name();
    WCUP_VALSTR(n ? n : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_context_compressor_name");
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    run_one();
    return 0;
}
