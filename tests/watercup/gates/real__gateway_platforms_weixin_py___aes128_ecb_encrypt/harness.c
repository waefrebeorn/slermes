/* gate: real__gateway_platforms_weixin_py___aes128_ecb_encrypt — GAP sentinel for _aes128_ecb_encrypt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_aes128_ecb_encrypt";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _aes128_ecb_encrypt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_aes128_ecb_encrypt");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_aes128_ecb_encrypt");
    }
    return 0;
}
