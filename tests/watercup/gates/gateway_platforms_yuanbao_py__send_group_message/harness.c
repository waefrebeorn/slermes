/* gate: gateway_platforms_yuanbao_py__send_group_message — traced copy of yb_send_group_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_send_group_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_send_group_message");
    int result = yb_send_group_message(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_send_group_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_send_group_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
