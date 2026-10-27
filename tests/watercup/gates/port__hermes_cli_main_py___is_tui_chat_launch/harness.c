/* gate: port__hermes_cli_main_py___is_tui_chat_launch — traced copy of _is_tui_chat_launch (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int _is_tui_chat_launch(int tui_flag, const char *hermes_tui_env);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_tui_chat_launch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_tui_chat_launch");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _is_tui_chat_launch((int)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_tui_chat_launch");
    }
    return 0;
}
