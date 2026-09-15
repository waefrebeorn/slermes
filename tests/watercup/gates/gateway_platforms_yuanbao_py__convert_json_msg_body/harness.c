/* gate: gateway_platforms_yuanbao_py__convert_json_msg_body — traced copy of yb_convert_json_msg_body */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_convert_json_msg_body(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_convert_json_msg_body");
    int result = yb_convert_json_msg_body(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_convert_json_msg_body");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_convert_json_msg_body <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
