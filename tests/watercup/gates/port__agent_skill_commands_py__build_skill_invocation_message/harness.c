/* gate: port__agent_skill_commands_py__build_skill_invocation_message — traced copy of skc_build_skill_invocation_message (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * skc_build_skill_invocation_message(const char *command, const char *args_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skc_build_skill_invocation_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("skc_build_skill_invocation_message");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = skc_build_skill_invocation_message(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("skc_build_skill_invocation_message");
    }
    return 0;
}
