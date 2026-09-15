/* gate: agent_context_references_py__format_reference_value — traced copy of agent_context_references_format_reference_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_context_references_format_reference_value(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_context_references_format_reference_value");
    int result = agent_context_references_format_reference_value(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_context_references_format_reference_value");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_context_references_format_reference_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
