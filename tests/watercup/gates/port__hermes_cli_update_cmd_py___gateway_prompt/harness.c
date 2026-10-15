/* gate: port__hermes_cli_update_cmd_py___gateway_prompt — traced copy of uc_gateway_prompt (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * uc_gateway_prompt(const char *default_home, const char *prompt_text,
                               const char *default_answer, double timeout_sec);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_gateway_prompt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_gateway_prompt");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = uc_gateway_prompt((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), strtod(argv[2 + (i - 2 + 3) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_gateway_prompt");
    }
    return 0;
}
