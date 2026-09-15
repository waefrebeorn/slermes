/* gate: yuanbao_encode_forward_msg_data — links the REAL C port
 * src/hermes_cli/yuanbao_proto_helpers.c::yuanbao_proto_encode_forward_msg_data
 * (+ its static encode helpers). LIVE twin:
 * gateway/platforms/yuanbao_proto.py::encode_forward_msg_data.
 *
 * Input argv: one ForwardMsgData JSON object per input.
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: data is an object     B2: begin_time truthy   B3: end_time truthy
 *   B4: nick_name non-empty   B5: msg list non-empty
 *   VSTR(hex of encoded protobuf bytes)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

unsigned char *yuanbao_proto_encode_forward_msg_data(const char *data_json,
                                                     size_t *out_len);

static void run_one(const char *data_json) {
    json_t *d = data_json ? json_parse(data_json, NULL) : NULL;
    bool obj = d && d->type == JSON_OBJECT;

    WCUP_BEGIN("encode_forward_msg_data");
    WCUP_BRANCH(obj);                                               /* B1 */
    if (obj) {
        json_t *v;
        bool bt = false, et = false, nick = false, msgs = false;
        if ((v = json_obj_get(d, "begin_time")) && v->type == JSON_NUMBER)
            bt = (int)json_number_value(v) != 0;
        if ((v = json_obj_get(d, "end_time")) && v->type == JSON_NUMBER)
            et = (int)json_number_value(v) != 0;
        if ((v = json_obj_get(d, "nick_name")) && v->type == JSON_STRING)
            nick = json_string_value(v)[0] != 0;
        json_t *m;
        if ((m = json_obj_get(d, "msg")) && m->type == JSON_ARRAY)
            msgs = json_array_size(m) > 0;
        WCUP_BRANCH(bt);                                            /* B2 */
        WCUP_BRANCH(et);                                            /* B3 */
        WCUP_BRANCH(nick);                                          /* B4 */
        WCUP_BRANCH(msgs);                                          /* B5 */
    }

    size_t out_len = 0;
    unsigned char *r = obj ? yuanbao_proto_encode_forward_msg_data(data_json, &out_len) : NULL;
    char hex[8192];
    for (size_t i = 0; i < out_len && i * 2 + 1 < sizeof hex; i++)
        snprintf(hex + i * 2, 3, "%02x", r[i]);
    hex[out_len * 2 < sizeof hex ? out_len * 2 : sizeof hex - 1] = '\0';
    WCUP_VALSTR(hex);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    if (d) json_free(d);
    WCUP_END("encode_forward_msg_data");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <data_json...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
