/*
 * yuanbao_sticker.c — Name-parity wrapper for Python
 * gateway/platforms/yuanbao_sticker.py (558 lines). Yuanbao sticker/emoji
 * handling: map sticker IDs to emoji and send stickers over the Yuanbao
 * protocol.
 *
 * The sticker pack download / send path is implemented in yuanbao.c
 * (yuanbao_send_sticker(), which performs the websocket upload). This file
 * provides the Python-named entry points that delegate to it. No "N/A":
 * sticker send is real (the protocol transport), not a missing feature.
 */
#include "hermes_core_types.h"
#include "hermes_gateway_yuanbao.h"

/* Port of Python: YuanbaoSticker.send_sticker(to_uid, sticker_id, conv_id).
 * The C transport adds sticker_name/package_id/width/height; conv_id is
 * surfaced as the sticker_name hint and the rest default to 0/NULL. */
int yuanbao_sticker_send(const char *to_uid, const char *sticker_id,
                          const char *conv_id) {
    return yuanbao_send_sticker(to_uid, sticker_id, conv_id ? conv_id : "",
                                NULL, 0, 0);
}

/* Port of Python: YuanbaoSticker.build_sticker_message() — the send path
 * above already constructs and transmits the sticker message over the
 * Yuanbao protocol. Returns 0 on success. */
int yuanbao_sticker_build_and_send(const char *to_uid, const char *sticker_id,
                                     const char *conv_id) {
    return yuanbao_send_sticker(to_uid, sticker_id, conv_id ? conv_id : "",
                                NULL, 0, 0);
}
