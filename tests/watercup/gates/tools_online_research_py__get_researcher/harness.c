/* gate: tools_online_research_py__get_researcher — traced copy of tools_online_research_get_researcher */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_online_research_get_researcher(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_online_research_get_researcher");
    int result = tools_online_research_get_researcher(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_online_research_get_researcher");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_online_research_get_researcher <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
