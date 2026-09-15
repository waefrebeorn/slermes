/* gate: hermes_cli_web_server_py__set_dashboard_font — traced copy of ws_theme_font_choice_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ws_theme_font_choice_allowed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_theme_font_choice_allowed");
    bool result = ws_theme_font_choice_allowed(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_theme_font_choice_allowed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_theme_font_choice_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
