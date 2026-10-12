/* gate: port__tools_todo_tool_py__check_todo_requirements — traced copy of todo_tool_check_requirements (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool todo_tool_check_requirements(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s todo_tool_check_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("todo_tool_check_requirements");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = todo_tool_check_requirements();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("todo_tool_check_requirements");
    }
    return 0;
}
