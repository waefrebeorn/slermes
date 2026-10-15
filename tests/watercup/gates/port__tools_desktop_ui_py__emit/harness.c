/* gate: port__tools_desktop_ui_py__emit — traced copy of tools_desktop_ui_emit (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool tools_desktop_ui_emit(const char *event, const char *payload_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_desktop_ui_emit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_desktop_ui_emit");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = tools_desktop_ui_emit(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tools_desktop_ui_emit");
    }
    return 0;
}
