/* gate: port__tools_approval_py____init__ — traced copy of apr2_init (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * apr2_init(const char *data_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("apr2_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = apr2_init(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("apr2_init");
    }
    return 0;
}
