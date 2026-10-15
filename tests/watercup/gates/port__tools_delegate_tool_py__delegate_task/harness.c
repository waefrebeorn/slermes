/* gate: port__tools_delegate_tool_py__delegate_task — traced copy of dlg_delegate_task (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * dlg_delegate_task(const char *tasks_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dlg_delegate_task <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dlg_delegate_task");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = dlg_delegate_task(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dlg_delegate_task");
    }
    return 0;
}
