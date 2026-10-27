/* gate: port__agent_display_py__set_tool_preview_max_len — traced copy of disp_set_tool_preview_max_len (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long disp_set_tool_preview_max_len(const char *n);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_set_tool_preview_max_len <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("disp_set_tool_preview_max_len");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = disp_set_tool_preview_max_len(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("disp_set_tool_preview_max_len");
    }
    return 0;
}
