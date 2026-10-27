/* gate: port__gateway_platforms_yuanbao_proto_py__encode_send_group_heartbeat — traced copy of yuanbao_proto_encode_send_group_heartbeat (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * yuanbao_proto_encode_send_group_heartbeat(const char *from_account, const char *group_code,
    uint64_t heartbeat, uint64_t send_time, size_t *out_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yuanbao_proto_encode_send_group_heartbeat <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("yuanbao_proto_encode_send_group_heartbeat");
    size_t _o4 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = yuanbao_proto_encode_send_group_heartbeat(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), &_o4);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o4); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("yuanbao_proto_encode_send_group_heartbeat");
    }
    return 0;
}
