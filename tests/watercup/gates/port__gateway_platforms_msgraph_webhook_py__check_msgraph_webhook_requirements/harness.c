/* gate: port__gateway_platforms_msgraph_webhook_py__check_msgraph_webhook_requirements — traced copy of msgraph_webhook_check_requirements (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool msgraph_webhook_check_requirements(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msgraph_webhook_check_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("msgraph_webhook_check_requirements");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = msgraph_webhook_check_requirements();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("msgraph_webhook_check_requirements");
    }
    return 0;
}
