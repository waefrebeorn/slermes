/* gate: port__hermes_cli_plugins_py___resolve_hook_callback_timeout — traced copy of _resolve_hook_callback_timeout (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _resolve_hook_callback_timeout(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _resolve_hook_callback_timeout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_resolve_hook_callback_timeout");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _resolve_hook_callback_timeout();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_resolve_hook_callback_timeout");
    }
    return 0;
}
