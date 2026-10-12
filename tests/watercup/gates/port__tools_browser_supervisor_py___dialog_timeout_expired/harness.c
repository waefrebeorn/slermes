/* gate: port__tools_browser_supervisor_py___dialog_timeout_expired — traced copy of browser_supervisor_dialog_timeout_expired (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void browser_supervisor_dialog_timeout_expired(const char *dialog_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_supervisor_dialog_timeout_expired <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("browser_supervisor_dialog_timeout_expired");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    browser_supervisor_dialog_timeout_expired(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("browser_supervisor_dialog_timeout_expired");
    }
    return 0;
}
