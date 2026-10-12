/* gate: port__tools_code_execution_tool_py__generate_hermes_tools_module — traced copy of code_exec_generate_hermes_tools_module (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void code_exec_generate_hermes_tools_module(int server_fd);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s code_exec_generate_hermes_tools_module <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("code_exec_generate_hermes_tools_module");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    code_exec_generate_hermes_tools_module(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("code_exec_generate_hermes_tools_module");
    }
    return 0;
}
