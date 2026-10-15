/* gate: port__hermes_cli_session_lost_and_found_py__find_sqlite3_cli_refusal — traced copy of find_sqlite3_cli_refusal (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * find_sqlite3_cli_refusal(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s find_sqlite3_cli_refusal <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("find_sqlite3_cli_refusal");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = find_sqlite3_cli_refusal();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("find_sqlite3_cli_refusal");
    }
    return 0;
}
