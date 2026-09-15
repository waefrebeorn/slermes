/* gate: gateway_platforms_weixin_py__qr_login — traced copy of wx_qr_login */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wx_qr_login(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wx_qr_login");
    int result = wx_qr_login(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wx_qr_login");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wx_qr_login <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
