/* gate: port__hermes_cli_setup_py___apply_default_agent_settings — traced copy of stp_apply_default_agent_settings (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int stp_apply_default_agent_settings(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_apply_default_agent_settings <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stp_apply_default_agent_settings");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = stp_apply_default_agent_settings();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stp_apply_default_agent_settings");
    }
    return 0;
}
