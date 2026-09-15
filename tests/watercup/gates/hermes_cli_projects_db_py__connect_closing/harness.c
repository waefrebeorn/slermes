/* gate: hermes_cli_projects_db_py__connect_closing — traced copy of hermes_cli_projects_db_connect_closing */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_projects_db_connect_closing(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_projects_db_connect_closing");
    int result = hermes_cli_projects_db_connect_closing(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_projects_db_connect_closing");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_projects_db_connect_closing <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
