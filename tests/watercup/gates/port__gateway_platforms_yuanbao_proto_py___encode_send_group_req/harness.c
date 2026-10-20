/* gate: port__gateway_platforms_yuanbao_proto_py___encode_send_group_req — traced copy of yb_encode_send_group_req (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * yb_encode_send_group_req(const char *group_code, const char *from_account,
    const char *msg_body_json, const char *msg_id, const char *to_account, const char *random,
    uint64_t msg_seq, const char *ref_msg_id, const char *trace_id, size_t *out_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_encode_send_group_req <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("yb_encode_send_group_req");
    size_t _o9 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = yb_encode_send_group_req(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5], (int)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10), argv[2 + (i - 2 + 7) % 5], argv[2 + (i - 2 + 8) % 5], &_o9);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o9); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("yb_encode_send_group_req");
    }
    return 0;
}
