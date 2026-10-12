/* gate: port__cli_py___should_emit_cleanup_session_finalize — traced copy of cli_should_emit_cleanup_session_finalize (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool cli_should_emit_cleanup_session_finalize(const char *session_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_should_emit_cleanup_session_finalize <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_should_emit_cleanup_session_finalize");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = cli_should_emit_cleanup_session_finalize(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_should_emit_cleanup_session_finalize");
    }
    return 0;
}
