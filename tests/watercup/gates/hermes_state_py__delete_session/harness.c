/* gate: hermes_state_py__delete_session — traced copy of ws_sess_delete_session */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_sess_delete_session(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_sess_delete_session");
    int result = ws_sess_delete_session(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_sess_delete_session");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_sess_delete_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
