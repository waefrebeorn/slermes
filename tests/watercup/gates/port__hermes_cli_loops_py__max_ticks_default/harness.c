/* gate: port__hermes_cli_loops_py__max_ticks_default — traced copy of loops_max_ticks_default (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int loops_max_ticks_default(const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_max_ticks_default <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("loops_max_ticks_default");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = loops_max_ticks_default(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("loops_max_ticks_default");
    }
    return 0;
}
