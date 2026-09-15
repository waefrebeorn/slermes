/* gate: tools_skill_provenance_py__get_current_write_origin — traced copy of tools_skill_provenance_get_current_write_origin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_skill_provenance_get_current_write_origin(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_skill_provenance_get_current_write_origin");
    int result = tools_skill_provenance_get_current_write_origin(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_skill_provenance_get_current_write_origin");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_skill_provenance_get_current_write_origin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
