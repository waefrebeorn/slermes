/* gate: tools_online_research_py__research_model_benchmarks — traced copy of tools_online_research_research_model_benchmarks */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_online_research_research_model_benchmarks(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_online_research_research_model_benchmarks");
    int result = tools_online_research_research_model_benchmarks(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_online_research_research_model_benchmarks");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_online_research_research_model_benchmarks <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
