/* gate: port__tools_blueprints_py___schedule_to_string — traced copy of cli_tools_blueprints__schedule_to_string (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_tools_blueprints__schedule_to_string(const char *schedule_kind, const char *schedule_expr,
    int schedule_minutes, int schedule_seconds,
    char *result_out, size_t result_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_blueprints__schedule_to_string <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_blueprints__schedule_to_string");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_blueprints__schedule_to_string(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_blueprints__schedule_to_string");
    }
    return 0;
}
