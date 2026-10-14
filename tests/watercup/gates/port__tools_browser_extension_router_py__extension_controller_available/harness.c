/* gate: port__tools_browser_extension_router_py__extension_controller_available — traced copy of bcr_extension_controller_available (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int bcr_extension_controller_available(const char *action);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bcr_extension_controller_available <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("bcr_extension_controller_available");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = bcr_extension_controller_available(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("bcr_extension_controller_available");
    }
    return 0;
}
