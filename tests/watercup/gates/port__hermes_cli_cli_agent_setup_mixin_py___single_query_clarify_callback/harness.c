/* gate: port__hermes_cli_cli_agent_setup_mixin_py___single_query_clarify_callback — traced copy of _single_query_clarify_callback (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _single_query_clarify_callback(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _single_query_clarify_callback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_single_query_clarify_callback");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _single_query_clarify_callback(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_single_query_clarify_callback");
    }
    return 0;
}
