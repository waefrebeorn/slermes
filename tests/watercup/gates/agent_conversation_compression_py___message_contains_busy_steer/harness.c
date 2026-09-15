/* gate: agent_conversation_compression_py___message_contains_busy_steer — traced copy of _message_contains_busy_steer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _message_contains_busy_steer(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_message_contains_busy_steer");
    int result = _message_contains_busy_steer(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_message_contains_busy_steer");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _message_contains_busy_steer <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
