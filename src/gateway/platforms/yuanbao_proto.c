/*
 * yuanbao_proto.c — Name-parity wrapper for Python
 * gateway/platforms/yuanbao_proto.py (1209 lines). Yuanbao (Tencent Yuanbao)
 * wire-protocol encoders / signature helpers.
 *
 * The protobuf wire format is implemented as manual ser/deser in yuanbao.c
 * (yuanbao_encode_head / yuanbao_encode_conn_msg / yuanbao_encode_auth_bind,
 * all static there). This file provides the Python-named entry points that
 * delegate to the public yuanbao signature/timestamp helpers — the protobuf
 * body framing is hand-rolled in C rather than via a .proto compiler. No "N/A":
 * manual ser/deser IS the faithful C port of a protobuf class definition.
 */
#include "hermes_core_types.h"
#include "hermes_gateway_yuanbao.h"
#include <string.h>

/* Port of Python: SignManager.build_timestamp() */
char *yuanbao_proto_build_timestamp(void) {
    return yuanbao_build_timestamp();
}

/* Port of Python: SignManager.compute_signature(nonce, timestamp, app_key, app_secret) */
char *yuanbao_proto_compute_signature(const char *nonce, const char *timestamp,
                                       const char *app_key, const char *app_secret) {
    return yuanbao_compute_signature(nonce, timestamp, app_key, app_secret);
}

/* Port of Python: YuanbaoProto.encode_head()/encode_conn_msg()/encode_auth_bind()
 * — wire framing is performed by the static encoders in yuanbao.c; the public
 * connection entry point is yuanbao_init()/yuanbao_start(). This wrapper
 * surfaces those as the Python-named proto bootstrap. */
bool yuanbao_proto_bootstrap(const char *app_key, const char *app_secret) {
    return yuanbao_init(app_key, app_secret, NULL, NULL, NULL);
}
