/* gate: port__hermes_cli_focus_view_py__focus_statusbar_segment — traced copy of fv_focus_statusbar_segment (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * fv_focus_statusbar_segment(bool enabled);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fv_focus_statusbar_segment <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fv_focus_statusbar_segment");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = fv_focus_statusbar_segment((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fv_focus_statusbar_segment");
    }
    return 0;
}
