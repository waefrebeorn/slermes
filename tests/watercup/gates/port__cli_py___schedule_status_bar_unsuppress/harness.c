/* gate: port__cli_py___schedule_status_bar_unsuppress — traced copy of cl2_schedule_status_bar_unsuppress (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cl2_schedule_status_bar_unsuppress(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cl2_schedule_status_bar_unsuppress <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cl2_schedule_status_bar_unsuppress");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cl2_schedule_status_bar_unsuppress();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cl2_schedule_status_bar_unsuppress");
    }
    return 0;
}
