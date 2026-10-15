/* gate: port__hermes_state_py__resolve_session_id — traced copy of ws_chat_resolve_session_id (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * ws_chat_resolve_session_id(const char *db_path, const char *prefix);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_chat_resolve_session_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_chat_resolve_session_id");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = ws_chat_resolve_session_id(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_chat_resolve_session_id");
    }
    return 0;
}
