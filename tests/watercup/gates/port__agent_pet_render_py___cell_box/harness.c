/* gate: port__agent_pet_render_py___cell_box — traced copy of pet_cell_box (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void pet_cell_box(int frame_idx, int cell_w, int cell_h, int sheet_w,
                  int *out_x, int *out_y, int *out_w, int *out_h);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pet_cell_box <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pet_cell_box");
    int _o4 = 0;
    int _o5 = 0;
    int _o6 = 0;
    int _o7 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    pet_cell_box((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), &_o4, &_o5, &_o6, &_o7);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o4); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o5); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o6); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o7); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pet_cell_box");
    }
    return 0;
}
