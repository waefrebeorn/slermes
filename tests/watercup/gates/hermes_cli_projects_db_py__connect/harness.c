/* gate: hermes_cli_projects_db_py__connect — traced copy of pdb_connect */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pdb_connect(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pdb_connect");
    int result = pdb_connect(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pdb_connect");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pdb_connect <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
