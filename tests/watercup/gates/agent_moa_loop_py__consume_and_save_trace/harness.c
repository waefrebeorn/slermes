/* gate: agent_moa_loop_py__consume_and_save_trace — traced copy of moa_consume_and_save_trace */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int moa_consume_and_save_trace(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("moa_consume_and_save_trace");
    int result = moa_consume_and_save_trace(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("moa_consume_and_save_trace");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_consume_and_save_trace <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
