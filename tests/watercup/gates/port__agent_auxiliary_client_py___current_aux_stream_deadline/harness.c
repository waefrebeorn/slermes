/* gate: port__agent_auxiliary_client_py___current_aux_stream_deadline — traced copy of _current_aux_stream_deadline (double_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double _current_aux_stream_deadline(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _current_aux_stream_deadline <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_current_aux_stream_deadline");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = _current_aux_stream_deadline((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_current_aux_stream_deadline");
    }
    return 0;
}
