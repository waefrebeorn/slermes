/* gate: agent_auxiliary_client_py___anthropic_event_has_content — traced copy of _anthropic_event_has_content */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _anthropic_event_has_content(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_anthropic_event_has_content");
    int result = _anthropic_event_has_content(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_anthropic_event_has_content");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _anthropic_event_has_content <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
