/* gate: port__agent_pet_render_py___frame_is_blank — traced copy of pet_frame_is_blank (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool pet_frame_is_blank(const unsigned char *rgba, size_t len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pet_frame_is_blank <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pet_frame_is_blank");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = pet_frame_is_blank(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pet_frame_is_blank");
    }
    return 0;
}
