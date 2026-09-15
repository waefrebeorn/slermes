/* gate: tools_async_delegation_py___push_completion_event — traced copy of adel_u_push_completion_event */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int adel_u_push_completion_event(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("adel_u_push_completion_event");
    int result = adel_u_push_completion_event(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("adel_u_push_completion_event");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s adel_u_push_completion_event <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
