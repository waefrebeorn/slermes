/* gate: gateway_platforms_weixin_py___looks_like_heading_line_for_weixin — traced copy of weixin_looks_like_heading_line */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool weixin_looks_like_heading_line(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("weixin_looks_like_heading_line");
    bool result = weixin_looks_like_heading_line(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("weixin_looks_like_heading_line");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s weixin_looks_like_heading_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
