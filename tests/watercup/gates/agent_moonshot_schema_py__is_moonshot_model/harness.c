/* gate: agent_moonshot_schema_py__is_moonshot_model — traced copy of msc_is_moonshot_model */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool msc_is_moonshot_model(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msc_is_moonshot_model");
    bool result = msc_is_moonshot_model(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msc_is_moonshot_model");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msc_is_moonshot_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
