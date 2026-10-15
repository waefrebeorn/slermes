/* gate: port__agent_pet_render_py___snap_frames_to_cell_grid — traced copy of pet_snap_frames_to_cell_grid (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void pet_snap_frames_to_cell_grid(int *x, int *y, int *w, int *h, int cell_w, int cell_h);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pet_snap_frames_to_cell_grid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pet_snap_frames_to_cell_grid");
    int _o0 = 0;
    int _o1 = 0;
    int _o2 = 0;
    int _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    pet_snap_frames_to_cell_grid(&_o0, &_o1, &_o2, &_o3, (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o0); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o2); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pet_snap_frames_to_cell_grid");
    }
    return 0;
}
