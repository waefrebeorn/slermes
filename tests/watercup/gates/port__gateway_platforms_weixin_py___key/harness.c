/* gate: port__gateway_platforms_weixin_py___key — traced copy of parse_aes_key (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int parse_aes_key(const char *aes_key_b64, unsigned char *out, size_t *out_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_aes_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("parse_aes_key");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = parse_aes_key(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_aes_key");
    }
    return 0;
}
