/* gate: port__agent_bedrock_adapter_py__is_stale_connection_error — traced copy of is_stale_connection_error (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool is_stale_connection_error(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_stale_connection_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("is_stale_connection_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = is_stale_connection_error();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_stale_connection_error");
    }
    return 0;
}
