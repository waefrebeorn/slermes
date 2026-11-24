/*
 * noc_w25_telegramme_bdc44d.h - C surface for upstream
 * plugins/platforms/telegram/telegram_media_retry.py
 * (gate w25_telegramme_bdc44d).
 *
 * The transient-failure policy for one native Telegram media upload: repeat the
 * SAME request only while the failure proves nothing was posted, and report an
 * upload that may already be in the chat as ambiguous instead of resending it.
 * The PTB exception classes, the socket, the upload itself and the log sink are
 * seams -- the classifier's verdict arrives as a fixed ``kind``.
 */
#ifndef NOC_W25_TELEGRAMME_BDC44D_H
#define NOC_W25_TELEGRAMME_BDC44D_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_TELEGRAMME_BDC44D_SAME_REQUEST_ATTEMPTS 3
#define W25_TELEGRAMME_BDC44D_REQUEST_ERROR_ATTR "_hermes_telegram_media_request_error"

/* {"request_error":bool,"kind":str} -> may the upload already be in the chat. */
json_t *w25_telegramme_bdc44d_request_may_have_delivered(const json_t *state);

/* {"request_error","kind","error_text","what","as_failure"} -> the ambiguous
 * SendResult dict, or null when the failure is a definite one. */
json_t *w25_telegramme_bdc44d_unconfirmed_media_result(const json_t *state);

/* {"attempts":[{"ok":bool,"kind":str,...}],"reset":bool} -> the retry trace:
 * {"status","attempts","delays":[..],"resets":N,"error_attr":bool}. */
json_t *w25_telegramme_bdc44d_retry_plan(const json_t *state);

/* The module/classifier constants, as an object. */
json_t *w25_telegramme_bdc44d_constants(void);

#ifdef __cplusplus
}
#endif
#endif
