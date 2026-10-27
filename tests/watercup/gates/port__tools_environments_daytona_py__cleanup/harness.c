/* gate: port__tools_environments_daytona_py__cleanup — traced copy of daytona_delete_sandbox (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool daytona_delete_sandbox(const char *sandbox_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s daytona_delete_sandbox <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("daytona_delete_sandbox");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = daytona_delete_sandbox(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("daytona_delete_sandbox");
    }
    return 0;
}
