/* gate: gateway_platforms_weixin_py___upload_ciphertext — traced copy of wx_u_upload_ciphertext */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wx_u_upload_ciphertext(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wx_u_upload_ciphertext");
    int result = wx_u_upload_ciphertext(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wx_u_upload_ciphertext");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wx_u_upload_ciphertext <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
