/* gate: port__tools_computer_use_cua_backend_py___image_dimensions_from_bytes — traced copy of PNG (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void PNG(const unsigned char *raw, size_t n,
                                             int *out_w, int *out_h);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s PNG <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("PNG");
    int _o2 = 0;
    int _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    PNG(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), &_o2, &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o2); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("PNG");
    }
    return 0;
}
