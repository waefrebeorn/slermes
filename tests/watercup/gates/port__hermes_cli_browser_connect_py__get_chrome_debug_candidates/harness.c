/* gate: port__hermes_cli_browser_connect_py__get_chrome_debug_candidates — traced copy of get_chrome_debug_candidates (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int get_chrome_debug_candidates(const char *url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_chrome_debug_candidates <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("get_chrome_debug_candidates");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = get_chrome_debug_candidates(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("get_chrome_debug_candidates");
    }
    return 0;
}
