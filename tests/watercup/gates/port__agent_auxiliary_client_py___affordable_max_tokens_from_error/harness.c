/* gate: port__agent_auxiliary_client_py___affordable_max_tokens_from_error — traced copy of _affordable_max_tokens_from_error (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _affordable_max_tokens_from_error(const char *error);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _affordable_max_tokens_from_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_affordable_max_tokens_from_error");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _affordable_max_tokens_from_error(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_affordable_max_tokens_from_error");
    }
    return 0;
}
