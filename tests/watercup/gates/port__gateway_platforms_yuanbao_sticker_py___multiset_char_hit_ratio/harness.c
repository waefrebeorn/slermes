/* gate: port__gateway_platforms_yuanbao_sticker_py___multiset_char_hit_ratio — traced copy of cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio(const char *needle, const char *haystack);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_yuanbao_sticker__multiset_char_hit_ratio");
    }
    return 0;
}
