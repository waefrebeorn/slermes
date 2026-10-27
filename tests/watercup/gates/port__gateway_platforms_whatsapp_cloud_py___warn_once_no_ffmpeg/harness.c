/* gate: port__gateway_platforms_whatsapp_cloud_py___warn_once_no_ffmpeg — traced copy of whatsapp_cloud_warn_once_no_ffmpeg (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void whatsapp_cloud_warn_once_no_ffmpeg(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s whatsapp_cloud_warn_once_no_ffmpeg <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("whatsapp_cloud_warn_once_no_ffmpeg");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    whatsapp_cloud_warn_once_no_ffmpeg();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("whatsapp_cloud_warn_once_no_ffmpeg");
    }
    return 0;
}
