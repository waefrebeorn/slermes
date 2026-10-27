/* gate: port__hermes_cli_loops_py__parse_interval_token — traced copy of loops_parse_interval_token (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int loops_parse_interval_token(const char *token);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_parse_interval_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("loops_parse_interval_token");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = loops_parse_interval_token(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("loops_parse_interval_token");
    }
    return 0;
}
