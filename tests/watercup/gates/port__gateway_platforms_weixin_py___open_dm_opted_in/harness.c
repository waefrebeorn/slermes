/* gate: port__gateway_platforms_weixin_py___open_dm_opted_in — traced copy of open_dm_opted_in (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool open_dm_opted_in(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s open_dm_opted_in <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("open_dm_opted_in");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = open_dm_opted_in();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("open_dm_opted_in");
    }
    return 0;
}
