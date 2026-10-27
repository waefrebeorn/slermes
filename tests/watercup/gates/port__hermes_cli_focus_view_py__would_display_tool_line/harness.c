/* gate: port__hermes_cli_focus_view_py__would_display_tool_line — traced copy of fv_would_display_tool_line (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool fv_would_display_tool_line(const char *mode_str,
                                 bool mode_is_bool,
                                 bool mode_bool_val,
                                 const char *function_name,
                                 const char *last_tool_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fv_would_display_tool_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fv_would_display_tool_line");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = fv_would_display_tool_line(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fv_would_display_tool_line");
    }
    return 0;
}
