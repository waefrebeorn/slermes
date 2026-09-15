/* gate: tools_kanban_tools_py___maybe_auto_subscribe — traced copy of kbt_maybe_auto_subscribe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool kbt_maybe_auto_subscribe(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kbt_maybe_auto_subscribe");
    bool result = kbt_maybe_auto_subscribe(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kbt_maybe_auto_subscribe");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kbt_maybe_auto_subscribe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
