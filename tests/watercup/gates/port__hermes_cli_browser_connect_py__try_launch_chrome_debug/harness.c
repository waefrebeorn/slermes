/* gate: port__hermes_cli_browser_connect_py__try_launch_chrome_debug — traced copy of hermes_cli_browser_connect_try_launch_chrome_debug (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int hermes_cli_browser_connect_try_launch_chrome_debug(int port, const char *system);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_browser_connect_try_launch_chrome_debug <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hermes_cli_browser_connect_try_launch_chrome_debug");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = hermes_cli_browser_connect_try_launch_chrome_debug((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hermes_cli_browser_connect_try_launch_chrome_debug");
    }
    return 0;
}
