/* gate: port__hermes_cli_focus_view_py__effective_tool_progress_mode — traced copy of fv_effective_tool_progress_mode (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * fv_effective_tool_progress_mode(bool focus_enabled,
                                             const char *configured_mode_str,
                                             bool mode_is_bool,
                                             bool mode_bool_val);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fv_effective_tool_progress_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fv_effective_tool_progress_mode");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = fv_effective_tool_progress_mode((int)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 2], (int)strtol(argv[2 + (i - 2 + 2) % 2], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 2], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fv_effective_tool_progress_mode");
    }
    return 0;
}
