/* gate: hermes_cli_curses_ui_py__radio_item_plain -> real radio_item_plain (JSON string or array of [text,style]). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *hermes_cli_curses_ui_radio_item_plain(const char *item_json);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hermes_cli_curses_ui_radio_item_plain");
        fflush(stdout);
        char *r = hermes_cli_curses_ui_radio_item_plain(argv[i]);
        WCUP_VALSTR(r ? r : "");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("hermes_cli_curses_ui_radio_item_plain");
        free(r);
    }
    return 0;
}
