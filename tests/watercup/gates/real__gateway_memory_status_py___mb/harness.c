/* gate: _mb(kib) -> int|None, fed the raw config token. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
const char *gw_memstat_mb_text(const char *token);
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_mb");
        WCUP_VALSTR(gw_memstat_mb_text(argv[i]));
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_mb");
    }
    return 0;
}
