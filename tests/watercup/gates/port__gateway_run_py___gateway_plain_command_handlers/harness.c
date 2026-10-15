/* gate: port__gateway_run_py___gateway_plain_command_handlers — traced copy of _gateway_plain_command_handlers (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _gateway_plain_command_handlers(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _gateway_plain_command_handlers <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_gateway_plain_command_handlers");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _gateway_plain_command_handlers();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_gateway_plain_command_handlers");
    }
    return 0;
}
