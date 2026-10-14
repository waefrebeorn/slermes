/* gate: port__cli_py___wait_for_oneshot_background_completions — traced copy of cli_wait_for_oneshot_background_completions (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void cli_wait_for_oneshot_background_completions(const char *task_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_wait_for_oneshot_background_completions <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_wait_for_oneshot_background_completions");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_wait_for_oneshot_background_completions(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_wait_for_oneshot_background_completions");
    }
    return 0;
}
