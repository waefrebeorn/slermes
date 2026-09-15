/* gate: gateway_platforms_yuanbao_py___extract_bot_mention_text — traced copy of yb_u_extract_bot_mention_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_u_extract_bot_mention_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_u_extract_bot_mention_text");
    int result = yb_u_extract_bot_mention_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_u_extract_bot_mention_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_u_extract_bot_mention_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
