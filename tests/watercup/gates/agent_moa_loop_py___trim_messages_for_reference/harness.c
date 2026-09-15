/* gate: agent_moa_loop_py___trim_messages_for_reference — traced copy of moa_u_trim_messages_for_reference */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int moa_u_trim_messages_for_reference(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("moa_u_trim_messages_for_reference");
    int result = moa_u_trim_messages_for_reference(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("moa_u_trim_messages_for_reference");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_u_trim_messages_for_reference <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
