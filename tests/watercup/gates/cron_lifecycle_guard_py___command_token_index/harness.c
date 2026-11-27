/* gate: cron_lifecycle_guard_py___command_token_index — traced copy of cron_lifecycle_command_token_index (strlist) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cron_lifecycle_command_token_index(char *const *segment);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_lifecycle_command_token_index <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cron_lifecycle_command_token_index");
    char *_tl[2];
    _tl[0] = argv[i];
    _tl[1] = NULL;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cron_lifecycle_command_token_index(_tl);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cron_lifecycle_command_token_index");
    }
    return 0;
}
