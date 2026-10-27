/* gate: port__agent_auxiliary_client_py___should_skip_same_provider_retry — traced copy of _should_skip_same_provider_retry (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int _should_skip_same_provider_retry(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _should_skip_same_provider_retry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_should_skip_same_provider_retry");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _should_skip_same_provider_retry();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_should_skip_same_provider_retry");
    }
    return 0;
}
