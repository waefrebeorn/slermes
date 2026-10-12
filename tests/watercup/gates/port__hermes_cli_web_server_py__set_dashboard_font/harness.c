/* gate: port__hermes_cli_web_server_py__set_dashboard_font — traced copy of ws_theme_font_choice_allowed (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool ws_theme_font_choice_allowed(const char *font_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_theme_font_choice_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_theme_font_choice_allowed");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = ws_theme_font_choice_allowed(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ws_theme_font_choice_allowed");
    }
    return 0;
}
