/* gate: port__hermes_cli_foreign_sessions_py___merge_turns — traced copy of foreign_sessions_merge_turns (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * foreign_sessions_merge_turns(const char *raw_turns_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s foreign_sessions_merge_turns <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("foreign_sessions_merge_turns");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = foreign_sessions_merge_turns(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("foreign_sessions_merge_turns");
    }
    return 0;
}
