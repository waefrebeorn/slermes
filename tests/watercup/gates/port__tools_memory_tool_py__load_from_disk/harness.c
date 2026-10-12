/* gate: port__tools_memory_tool_py__load_from_disk — GAP sentinel for load_from_disk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "load_from_disk";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s load_from_disk <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("load_from_disk");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("load_from_disk");
    }
    return 0;
}
