/* gate: port__hermes_cli_projects_db_py__list_projects — GAP sentinel for list_projects */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "list_projects";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s list_projects <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("list_projects");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("list_projects");
    }
    return 0;
}
