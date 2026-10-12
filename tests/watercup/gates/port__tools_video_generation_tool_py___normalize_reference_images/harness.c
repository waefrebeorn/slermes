/* gate: port__tools_video_generation_tool_py___normalize_reference_images — traced copy of _normalize_reference_images (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _normalize_reference_images(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _normalize_reference_images <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_normalize_reference_images");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _normalize_reference_images(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_normalize_reference_images");
    }
    return 0;
}
