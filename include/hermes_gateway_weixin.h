/**
 * @file hermes_gateway_weixin.h
 * @brief Weixin (iLink Bot API for WeChat) platform declarations.
 */
#ifndef HERMES_GATEWAY_WEIXIN_H
#define HERMES_GATEWAY_WEIXIN_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  weixin — iLink Bot API for WeChat
 * ================================================================ */

bool weixin_init(const char *token, const char *account_id);
void weixin_start(void);
void weixin_stop(void);

/* P113: Weixin extended send API */
void weixin_send_text(const char *chat_id, const char *text,
                       const char *context_token);
void weixin_send_markdown(const char *chat_id, const char *text,
                           const char *context_token);
void weixin_send_image(const char *chat_id, const char *image_data,
                        int image_type, const char *context_token);
void weixin_send_video(const char *chat_id, const char *video_url,
                        const char *context_token);
void weixin_send_file(const char *chat_id, const char *file_url,
                       const char *filename, const char *context_token);

/* Split content into weixin-delivery segments (Python:
 * _split_text_for_weixin_delivery). Caller frees each string + the array. */
char **weixin_split_text_for_weixin_delivery(const char *content, int max_length,
                                             bool split_per_line, int *out_count);

#endif /* HERMES_GATEWAY_WEIXIN_H */