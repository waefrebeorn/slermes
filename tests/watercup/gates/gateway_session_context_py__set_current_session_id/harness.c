/* gate: gateway_session_context_py__set_current_session_id — traced copy of sct_set_current_session_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sct_set_current_session_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sct_set_current_session_id");
    int result = sct_set_current_session_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sct_set_current_session_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sct_set_current_session_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
