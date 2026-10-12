/* gate: port__tools_approval_py___is_cron_approval_context — traced copy of apr2_is_cron_approval_context (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool apr2_is_cron_approval_context(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_is_cron_approval_context <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("apr2_is_cron_approval_context");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = apr2_is_cron_approval_context();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("apr2_is_cron_approval_context");
    }
    return 0;
}
