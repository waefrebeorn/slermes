/* gate: port__gateway_stream_consumer_py__escape_code_fences_for_display — traced copy of sc_escape_code_fences_for_display (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void sc_escape_code_fences_for_display(char *buf, size_t sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sc_escape_code_fences_for_display <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sc_escape_code_fences_for_display");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    sc_escape_code_fences_for_display();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sc_escape_code_fences_for_display");
    }
    return 0;
}
