/* gate: agent_curator_py___needle_in_path_component — traced copy of cur_needle_in_path_component */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cur_needle_in_path_component(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cur_needle_in_path_component");
    int result = cur_needle_in_path_component(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cur_needle_in_path_component");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cur_needle_in_path_component <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
