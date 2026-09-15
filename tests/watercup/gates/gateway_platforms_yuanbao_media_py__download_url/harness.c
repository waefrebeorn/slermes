/* gate: gateway_platforms_yuanbao_media_py__download_url — traced copy of cli_gateway_platforms_yuanbao_media_download_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_gateway_platforms_yuanbao_media_download_url(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_gateway_platforms_yuanbao_media_download_url");
    int result = cli_gateway_platforms_yuanbao_media_download_url(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_gateway_platforms_yuanbao_media_download_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_yuanbao_media_download_url <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
