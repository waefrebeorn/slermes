/* gate: cli_py___persist_active_session_before_close — traced copy of cl2_persist_active_session_before_close */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cl2_persist_active_session_before_close(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cl2_persist_active_session_before_close");
    int result = cl2_persist_active_session_before_close(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cl2_persist_active_session_before_close");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cl2_persist_active_session_before_close <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
