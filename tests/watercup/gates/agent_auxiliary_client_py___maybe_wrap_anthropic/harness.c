/* gate: agent_auxiliary_client_py___maybe_wrap_anthropic — traced copy of aux__maybe_wrap_anthropic */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int aux__maybe_wrap_anthropic(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("aux__maybe_wrap_anthropic");
    int result = aux__maybe_wrap_anthropic(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("aux__maybe_wrap_anthropic");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s aux__maybe_wrap_anthropic <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
