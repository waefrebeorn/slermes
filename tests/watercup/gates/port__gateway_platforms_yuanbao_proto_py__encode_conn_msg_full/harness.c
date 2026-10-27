/* gate: port__gateway_platforms_yuanbao_proto_py__encode_conn_msg_full — traced copy of yuanbao_proto_encode_conn_msg_full (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * yuanbao_proto_encode_conn_msg_full(int cmd_type, const char *cmd, uint64_t seq_no,
    const char *msg_id, const char *module, const unsigned char *data, size_t data_len, int need_ack,
    size_t *out_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yuanbao_proto_encode_conn_msg_full <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("yuanbao_proto_encode_conn_msg_full");
    size_t _o8 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = yuanbao_proto_encode_conn_msg_full((int)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5], (long)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 7) % 5], NULL, 10), &_o8);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o8); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("yuanbao_proto_encode_conn_msg_full");
    }
    return 0;
}
