/* gate: port__hermes_cli_setup_py__setup_agent_settings — traced copy of setup_agent_settings (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void setup_agent_settings(char *reasoning, size_t rz_sz,
                                 bool *collaboration, bool *save_trajectories);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s setup_agent_settings <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("setup_agent_settings");
    char _s0[256]; _s0[0] = 0;
    bool _o2 = 0;
    bool _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    setup_agent_settings(_s0, sizeof(_s0), &_o2, &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_s0);
    WCUP_VALSTR(_o2 ? "true" : "false");
    WCUP_VALSTR(_o3 ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("setup_agent_settings");
    }
    return 0;
}
