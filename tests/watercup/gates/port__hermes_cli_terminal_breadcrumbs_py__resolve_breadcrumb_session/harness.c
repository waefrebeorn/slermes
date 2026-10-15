/* gate: port__hermes_cli_terminal_breadcrumbs_py__resolve_breadcrumb_session — traced copy of resolve_breadcrumb_session (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * resolve_breadcrumb_session(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_breadcrumb_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("resolve_breadcrumb_session");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = resolve_breadcrumb_session();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_breadcrumb_session");
    }
    return 0;
}
