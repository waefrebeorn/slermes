/* gate: port__tools_approval_py__prompt_dangerous_approval — traced copy of apr2_prompt_dangerous_approval (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * apr2_prompt_dangerous_approval(const char *command, bool allow_anyway);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_prompt_dangerous_approval <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("apr2_prompt_dangerous_approval");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = apr2_prompt_dangerous_approval((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("apr2_prompt_dangerous_approval");
    }
    return 0;
}
