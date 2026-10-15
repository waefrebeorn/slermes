/* gate: port__gateway_platforms_yuanbao_media_py___parse_jpeg_size — traced copy of cli_gateway_platforms_yuanbao_media__parse_jpeg_size (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_gateway_platforms_yuanbao_media__parse_jpeg_size(const unsigned char *data, int data_len, int *width, int *height);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_yuanbao_media__parse_jpeg_size <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_yuanbao_media__parse_jpeg_size");
    int _o2 = 0;
    int _o3 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_gateway_platforms_yuanbao_media__parse_jpeg_size(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), &_o2, &_o3);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o2); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o3); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_yuanbao_media__parse_jpeg_size");
    }
    return 0;
}
