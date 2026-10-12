/* gate: port__hermes_cli_terminal_breadcrumbs_py__write_breadcrumb — traced copy of write_breadcrumb (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void write_breadcrumb(const char *session_id, const char *cwd);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s write_breadcrumb <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("write_breadcrumb");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    write_breadcrumb(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("write_breadcrumb");
    }
    return 0;
}
