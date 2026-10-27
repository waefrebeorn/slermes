/* gate: port__hermes_cli_focus_view_py__resolve_focus_arg — traced copy of fv_resolve_focus_arg (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * fv_resolve_focus_arg(const char *arg, bool current,
                                  bool *out_target);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fv_resolve_focus_arg <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fv_resolve_focus_arg");
    bool _o2 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = fv_resolve_focus_arg(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), &_o2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    WCUP_VALSTR(_o2 ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fv_resolve_focus_arg");
    }
    return 0;
}
