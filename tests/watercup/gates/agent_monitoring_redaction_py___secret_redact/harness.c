/* gate: agent_monitoring_redaction_py___secret_redact — traced copy of agent_monitoring_redaction_u_secret_redact */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_monitoring_redaction_u_secret_redact(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_monitoring_redaction_u_secret_redact");
    int result = agent_monitoring_redaction_u_secret_redact(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_monitoring_redaction_u_secret_redact");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_monitoring_redaction_u_secret_redact <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
