/* gate: yuanbao_proto_encode_head — traced copy of
 * src/hermes_cli/yuanbao_proto_helpers.c::yb_encode_head.
 * LIVE twin: gateway/platforms/yuanbao_proto.py::_encode_head.
 * Input format: "<cmd_type>\t<cmd>\t<seq_no>\t<msg_id>\t<module>\t<need_ack>\t<status>"
 * Branch ladder (same rung order both sides): one B per field presence:
 *   cmd_type, cmd, seq_no, msg_id, module, need_ack, status. V = hex of buffer.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "watercup.h"

static void put_varint(uint8_t *b, int *n, uint64_t v) {
    while (1) {
        uint8_t byte = (uint8_t)(v & 0x7F);
        v >>= 7;
        if (v) b[(*n)++] = (uint8_t)(byte | 0x80);
        else { b[(*n)++] = byte; break; }
    }
}
static void put_field_varint(uint8_t *b, int *n, int field, uint64_t v) {
    put_varint(b, n, ((uint64_t)field << 3) | 0);
    put_varint(b, n, v);
}
static void put_field_string(uint8_t *b, int *n, int field, const char *s) {
    put_varint(b, n, ((uint64_t)field << 3) | 2);
    size_t len = strlen(s);
    put_varint(b, n, (uint64_t)len);
    memcpy(b + *n, s, len); *n += (int)len;
}

static void trace_one(const char *input) {
    char tmp[1024]; snprintf(tmp, sizeof tmp, "%s", input);
    char *fields[8]; int nf = 0;
    char *p = tmp;
    fields[nf++] = p;
    for (char *q = tmp; *q && nf < 8; q++) {
        if (*q == '\t') { *q = '\0'; fields[nf++] = q + 1; }
    }
    while (nf < 7) fields[nf++] = (char *)"";
    int cmd_type = atoi(fields[0]);
    const char *cmd = fields[1];
    uint64_t seq_no = strtoull(fields[2], NULL, 10);
    const char *msg_id = fields[3];
    const char *module = fields[4];
    int need_ack = atoi(fields[5]);
    uint64_t status = strtoull(fields[6], NULL, 10);

    WCUP_BEGIN("_encode_head");
    uint8_t buf[2048]; int n = 0;
    if (cmd_type != 0) { WCUP_BRANCH(1); put_field_varint(buf, &n, 1, (uint64_t)cmd_type); }
    else WCUP_BRANCH(0);
    if (cmd && *cmd) { WCUP_BRANCH(1); put_field_string(buf, &n, 2, cmd); }
    else WCUP_BRANCH(0);
    if (seq_no != 0) { WCUP_BRANCH(1); put_field_varint(buf, &n, 3, seq_no); }
    else WCUP_BRANCH(0);
    if (msg_id && *msg_id) { WCUP_BRANCH(1); put_field_string(buf, &n, 4, msg_id); }
    else WCUP_BRANCH(0);
    if (module && *module) { WCUP_BRANCH(1); put_field_string(buf, &n, 5, module); }
    else WCUP_BRANCH(0);
    if (need_ack) { WCUP_BRANCH(1); put_field_varint(buf, &n, 6, 1); }
    else WCUP_BRANCH(0);
    if (status != 0) { WCUP_BRANCH(1); put_field_varint(buf, &n, 10, status & 0xFFFFFFFFULL); }
    else WCUP_BRANCH(0);

    char hex[4096];
    for (int i = 0; i < n && (size_t)(i * 2 + 2) < sizeof hex; i++)
        snprintf(hex + i * 2, 3, "%02x", buf[i]);
    hex[n * 2] = '\0';
    WCUP_VALSTR(hex);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_encode_head");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _encode_head <tsv>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
