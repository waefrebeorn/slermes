/* gate: port__tools_environments_base_py__get_activity_callback — traced copy of envb_get_activity_callback (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void * envb_get_activity_callback(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_get_activity_callback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("envb_get_activity_callback");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    envb_get_activity_callback();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("envb_get_activity_callback");
    }
    return 0;
}
