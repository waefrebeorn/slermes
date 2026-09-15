/* gate: gateway_platforms_yuanbao_py__parse_json_push — traced copy of yb_parse_json_push */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_parse_json_push(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_parse_json_push");
    int result = yb_parse_json_push(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_parse_json_push");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_parse_json_push <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
