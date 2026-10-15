/* gate: port__agent_display_py__get_cute_tool_message — traced copy of disp_get_cute_tool_message (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * disp_get_cute_tool_message(const char *tool_name, const char *args_json,
                                 double duration, const char *result);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_get_cute_tool_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("disp_get_cute_tool_message");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = disp_get_cute_tool_message((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), strtod(argv[2 + (i - 2 + 2) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("disp_get_cute_tool_message");
    }
    return 0;
}
