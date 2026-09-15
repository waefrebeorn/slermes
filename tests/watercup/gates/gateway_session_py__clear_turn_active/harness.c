/* gate: gateway_session_py__clear_turn_active — traced copy of session_clear_turn_active */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int session_clear_turn_active(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("session_clear_turn_active");
    int result = session_clear_turn_active(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("session_clear_turn_active");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_clear_turn_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
