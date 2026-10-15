/* gate: port__agent_pet_render_py___kitty_apc — traced copy of pet_kitty_apc (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * pet_kitty_apc(const char *data, size_t data_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pet_kitty_apc <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pet_kitty_apc");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = pet_kitty_apc((int)strtol(argv[i], NULL, 10), (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pet_kitty_apc");
    }
    return 0;
}
