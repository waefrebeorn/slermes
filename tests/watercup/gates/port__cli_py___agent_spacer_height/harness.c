/* gate: port__cli_py___agent_spacer_height — traced copy of cl2_agent_spacer_height (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long cl2_agent_spacer_height(bool agent_running);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cl2_agent_spacer_height <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cl2_agent_spacer_height");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = cl2_agent_spacer_height((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cl2_agent_spacer_height");
    }
    return 0;
}
