/* gate: port__tools_browser_camofox_py___adopt_existing_tab — traced copy of _adopt_existing_tab (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _adopt_existing_tab(const char *camofox_url, const char *url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _adopt_existing_tab <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_adopt_existing_tab");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _adopt_existing_tab(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_adopt_existing_tab");
    }
    return 0;
}
