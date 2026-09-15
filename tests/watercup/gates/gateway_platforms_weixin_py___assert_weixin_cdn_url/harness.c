/* gate: gateway_platforms_weixin_py___assert_weixin_cdn_url — traced copy of weixin_assert_cdn_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool weixin_assert_cdn_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("weixin_assert_cdn_url");
    bool result = weixin_assert_cdn_url(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("weixin_assert_cdn_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s weixin_assert_cdn_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
