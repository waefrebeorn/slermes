/* gate: hermes_cli_web_server_py___start_desktop_cron_ticker — traced copy of ws_start_desktop_cron_ticker */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_start_desktop_cron_ticker(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_start_desktop_cron_ticker");
    int result = ws_start_desktop_cron_ticker(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_start_desktop_cron_ticker");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_start_desktop_cron_ticker <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
