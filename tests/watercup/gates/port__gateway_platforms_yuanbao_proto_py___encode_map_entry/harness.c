/* gate: port__gateway_platforms_yuanbao_proto_py___encode_map_entry — traced copy of yuanbao_proto_encode_map_entry (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * yuanbao_proto_encode_map_entry(const char *key, const char *value, size_t *out_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yuanbao_proto_encode_map_entry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("yuanbao_proto_encode_map_entry");
    size_t _o2 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = yuanbao_proto_encode_map_entry(argv[i], argv[2 + (i - 2 + 1) % 5], &_o2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o2); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("yuanbao_proto_encode_map_entry");
    }
    return 0;
}
