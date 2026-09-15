/* gate: gateway_platforms_yuanbao_py__send_biz_request — traced copy of yb_send_biz_request */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_send_biz_request(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_send_biz_request");
    int result = yb_send_biz_request(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_send_biz_request");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_send_biz_request <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
