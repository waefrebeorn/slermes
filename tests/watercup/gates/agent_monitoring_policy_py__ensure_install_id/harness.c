/* gate: agent_monitoring_policy_py__ensure_install_id — traced copy of agent_monitoring_policy_ensure_install_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_monitoring_policy_ensure_install_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_monitoring_policy_ensure_install_id");
    int result = agent_monitoring_policy_ensure_install_id(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_monitoring_policy_ensure_install_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_monitoring_policy_ensure_install_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
