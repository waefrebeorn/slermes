/* gate: port__hermes_cli_linux_desktop_entry_py__render_desktop_entry — traced copy of lde_render_desktop_entry (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * lde_render_desktop_entry(const char *exec_command, const char *icon);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lde_render_desktop_entry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("lde_render_desktop_entry");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = lde_render_desktop_entry(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("lde_render_desktop_entry");
    }
    return 0;
}
