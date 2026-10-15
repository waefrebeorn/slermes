/* gate: port__hermes_cli_setup_py__setup_model_provider — traced copy of setup_model_provider (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int setup_model_provider(const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s setup_model_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("setup_model_provider");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = setup_model_provider(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("setup_model_provider");
    }
    return 0;
}
