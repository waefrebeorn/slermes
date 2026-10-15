/* gate: port__tools_browser_supervisor_py___on_event — traced copy of browser_supervisor_on_event (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void browser_supervisor_on_event(const char *method, const char *params_json, const char *session_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_supervisor_on_event <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("browser_supervisor_on_event");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    browser_supervisor_on_event();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("browser_supervisor_on_event");
    }
    return 0;
}
