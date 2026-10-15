/* gate: port__gateway_platforms_yuanbao_sticker_py___compact_text — traced copy of cli_gateway_platforms_yuanbao_sticker__compact_text (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char* cli_gateway_platforms_yuanbao_sticker__compact_text(const char *raw, char *buf, size_t bufsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_yuanbao_sticker__compact_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_yuanbao_sticker__compact_text");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char* result = cli_gateway_platforms_yuanbao_sticker__compact_text((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_yuanbao_sticker__compact_text");
    }
    return 0;
}
