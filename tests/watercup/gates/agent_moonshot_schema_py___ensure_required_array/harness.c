/* gate: agent_moonshot_schema_py___ensure_required_array — traced copy of agent_moonshot_schema_u_ensure_required_array */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_moonshot_schema_u_ensure_required_array(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_moonshot_schema_u_ensure_required_array");
    int result = agent_moonshot_schema_u_ensure_required_array(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_moonshot_schema_u_ensure_required_array");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_moonshot_schema_u_ensure_required_array <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
