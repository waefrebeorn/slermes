/* gate: port__hermes_cli_agent_import_py__import_agent_command — traced copy of ai_import_agent_command (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void ai_import_agent_command(const char *agent, const char *source,
                             bool dry_run, bool overwrite, bool auto_yes);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ai_import_agent_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ai_import_agent_command");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    ai_import_agent_command(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ai_import_agent_command");
    }
    return 0;
}
