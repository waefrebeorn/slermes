/* gate: port__hermes_cli_debug_py__collect_debug_report — traced copy of debug_collect_debug_report (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * debug_collect_debug_report(int log_lines, const char *dump_text,
                                 const char *agent_tail,
                                 const char *errors_tail,
                                 const char *gateway_tail,
                                 const char *gui_tail,
                                 const char *desktop_tail);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s debug_collect_debug_report <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("debug_collect_debug_report");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = debug_collect_debug_report((int)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5], argv[2 + (i - 2 + 6) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("debug_collect_debug_report");
    }
    return 0;
}
