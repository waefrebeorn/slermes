/* gate: port__gateway_relay_adapter_py___slack_unfurl_hints — traced copy of _slack_unfurl_hints (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _slack_unfurl_hints(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _slack_unfurl_hints <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_slack_unfurl_hints");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _slack_unfurl_hints();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_slack_unfurl_hints");
    }
    return 0;
}
