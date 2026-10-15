/* gate: port__hermes_cli_foreign_sessions_py__parse_codex_session — traced copy of foreign_sessions_parse_codex_session (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * foreign_sessions_parse_codex_session(const char *jsonl);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s foreign_sessions_parse_codex_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("foreign_sessions_parse_codex_session");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = foreign_sessions_parse_codex_session(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("foreign_sessions_parse_codex_session");
    }
    return 0;
}
