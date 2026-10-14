/* gate: port__gateway_platforms_signal_py___remember_sent_message_timestamp — traced copy of _remember_sent_message_timestamp (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _remember_sent_message_timestamp(const char* timestamp);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _remember_sent_message_timestamp <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_remember_sent_message_timestamp");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _remember_sent_message_timestamp(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_remember_sent_message_timestamp");
    }
    return 0;
}
