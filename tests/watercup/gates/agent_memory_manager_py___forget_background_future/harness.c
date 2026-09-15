/* gate: agent_memory_manager_py___forget_background_future — traced copy of mm_u_forget_background_future */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mm_u_forget_background_future(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mm_u_forget_background_future");
    int result = mm_u_forget_background_future(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mm_u_forget_background_future");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mm_u_forget_background_future <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
