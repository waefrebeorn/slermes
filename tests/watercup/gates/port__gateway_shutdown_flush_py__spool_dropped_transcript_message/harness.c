/* gate: port__gateway_shutdown_flush_py__spool_dropped_transcript_message — traced copy of spool_dropped_transcript_message (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * spool_dropped_transcript_message(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s spool_dropped_transcript_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("spool_dropped_transcript_message");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = spool_dropped_transcript_message();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("spool_dropped_transcript_message");
    }
    return 0;
}
