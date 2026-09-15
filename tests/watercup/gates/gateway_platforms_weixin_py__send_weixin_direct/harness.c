/* gate: gateway_platforms_weixin_py__send_weixin_direct — traced copy of wx_send_weixin_direct */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wx_send_weixin_direct(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wx_send_weixin_direct");
    int result = wx_send_weixin_direct(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wx_send_weixin_direct");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wx_send_weixin_direct <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
