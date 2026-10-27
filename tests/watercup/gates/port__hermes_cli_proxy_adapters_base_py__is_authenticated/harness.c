/* gate: port__hermes_cli_proxy_adapters_base_py__is_authenticated — traced copy of pxb_is_authenticated (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool pxb_is_authenticated(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pxb_is_authenticated <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pxb_is_authenticated");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = pxb_is_authenticated();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pxb_is_authenticated");
    }
    return 0;
}
