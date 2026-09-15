/* gate: agent_battery_py__read_battery — traced copy of agent_battery_read_battery */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_battery_read_battery(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_battery_read_battery");
    int result = agent_battery_read_battery(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_battery_read_battery");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_battery_read_battery <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
