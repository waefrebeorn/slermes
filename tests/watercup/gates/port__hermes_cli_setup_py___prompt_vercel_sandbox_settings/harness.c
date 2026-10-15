/* gate: port__hermes_cli_setup_py___prompt_vercel_sandbox_settings — traced copy of _prompt_vercel_sandbox_settings (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _prompt_vercel_sandbox_settings(const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _prompt_vercel_sandbox_settings <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_prompt_vercel_sandbox_settings");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _prompt_vercel_sandbox_settings();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_prompt_vercel_sandbox_settings");
    }
    return 0;
}
