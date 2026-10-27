/* gate: port__tools_react_to_message_tool_py___react_to_message_with_db — traced copy of _react_to_message_with_db (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _react_to_message_with_db(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _react_to_message_with_db <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_react_to_message_with_db");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _react_to_message_with_db();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_react_to_message_with_db");
    }
    return 0;
}
