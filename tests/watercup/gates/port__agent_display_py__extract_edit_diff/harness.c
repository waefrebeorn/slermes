/* gate: port__agent_display_py__extract_edit_diff — traced copy of disp_extract_edit_diff (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * disp_extract_edit_diff(const char *tool_name, const char *result);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_extract_edit_diff <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("disp_extract_edit_diff");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = disp_extract_edit_diff(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("disp_extract_edit_diff");
    }
    return 0;
}
