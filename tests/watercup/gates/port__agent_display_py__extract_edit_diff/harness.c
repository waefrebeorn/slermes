/* gate: port__agent_display_py__extract_edit_diff — traced copy of disp_extract_edit_diff (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool disp_extract_edit_diff(const char *diff_text);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_extract_edit_diff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("disp_extract_edit_diff");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = disp_extract_edit_diff(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("disp_extract_edit_diff");
    }
    return 0;
}
