/* gate: gateway_platforms_weixin_py__save_weixin_account — traced copy of wx_save_weixin_account */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wx_save_weixin_account(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wx_save_weixin_account");
    int result = wx_save_weixin_account(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wx_save_weixin_account");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wx_save_weixin_account <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
