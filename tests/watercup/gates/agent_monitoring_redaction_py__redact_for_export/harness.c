/* gate: agent_monitoring_redaction_py__redact_for_export — traced copy of agent_monitoring_redaction_redact_for_export */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_monitoring_redaction_redact_for_export(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_monitoring_redaction_redact_for_export");
    int result = agent_monitoring_redaction_redact_for_export(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_monitoring_redaction_redact_for_export");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_monitoring_redaction_redact_for_export <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
