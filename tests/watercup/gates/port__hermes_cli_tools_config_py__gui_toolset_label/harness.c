/* gate: port__hermes_cli_tools_config_py__gui_toolset_label — traced copy of gui_toolset_label (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * gui_toolset_label(const char *label);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gui_toolset_label <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("gui_toolset_label");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = gui_toolset_label(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gui_toolset_label");
    }
    return 0;
}
