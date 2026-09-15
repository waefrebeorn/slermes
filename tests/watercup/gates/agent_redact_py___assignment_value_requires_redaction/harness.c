/* gate: agent_redact_py___assignment_value_requires_redaction — traced copy of _assignment_value_requires_redaction */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _assignment_value_requires_redaction(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_assignment_value_requires_redaction");
    int result = _assignment_value_requires_redaction(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_assignment_value_requires_redaction");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _assignment_value_requires_redaction <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
