/* gate: port__tools_tool_result_storage_py__enforce_turn_budget — traced copy of cli_tools_tool_result_storage_enforce_turn_budget (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_tools_tool_result_storage_enforce_turn_budget(char *tool_contents[], int *tool_sizes[], int tool_count,
    int turn_budget, int *persisted_indices[], int *persisted_count);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_tool_result_storage_enforce_turn_budget <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_tool_result_storage_enforce_turn_budget");
    int _o1 = 0;
    int _o4 = 0;
    int _o5 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_tool_result_storage_enforce_turn_budget(argv[i], &_o1, (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), &_o4, &_o5);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o4); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o5); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_tool_result_storage_enforce_turn_budget");
    }
    return 0;
}
