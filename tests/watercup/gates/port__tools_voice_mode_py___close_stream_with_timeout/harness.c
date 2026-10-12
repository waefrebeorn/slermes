/* gate: port__tools_voice_mode_py___close_stream_with_timeout — traced copy of vmd_close_stream_with_timeout (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int vmd_close_stream_with_timeout(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vmd_close_stream_with_timeout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("vmd_close_stream_with_timeout");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = vmd_close_stream_with_timeout();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("vmd_close_stream_with_timeout");
    }
    return 0;
}
