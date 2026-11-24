/*
 * noc_w24_onboard_d18acb.h - C surface for upstream
 * plugins/platforms/qqbot/onboard.py (gate w24_onboard_d18acb).
 *
 * The value-level core of QQBot scan-to-configure: the QR target URL, the bind
 * task id extraction, the portal retcode check, the poll-response parse with
 * the BindStatus enum, the QR-expiry refresh decision and the onboarding
 * constants.  httpx, the portal, qrcode and time.sleep stay in the caller.
 */
#ifndef W24_ONBOARD_D18ACB_H
#define W24_ONBOARD_D18ACB_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BindStatus members. */
#define W24_ONBOARD_D18ACB_STATUS_COUNT 4
/* _MAX_REFRESHES + 1 tasks, at most, per qr_register(). */
#define W24_ONBOARD_D18ACB_MAX_TASKS 4

/* task_id -> the QR-code target URL (quote() plus QR_URL_TEMPLATE). */
json_t *w24_onboard_d18acb_url(const json_t *task_id);

/* portal response -> {"status", "status_name", "bot_appid",
 * "bot_encrypt_secret", "user_openid"}; null on the BindStatus ValueError or a
 * non-mapping ``data`` body. */
json_t *w24_onboard_d18acb_poll_result(const json_t *data);

/* (portal response, fail_msg) -> {"error": msg} on a non-zero retcode (the live
 * RuntimeError), else null. */
json_t *w24_onboard_d18acb_portal_error(const json_t *data, const json_t *fail_msg);

/* create-task response -> {"task_id": <value>} or {"error": ...} when the task
 * id is missing; null when the body is not a mapping. */
json_t *w24_onboard_d18acb_create_result(const json_t *data);

/* refresh_count -> {"action": "refresh", "refresh_number"} or
 * {"action": "give_up"} once _MAX_REFRESHES expiries were consumed. */
json_t *w24_onboard_d18acb_expiry(const json_t *refresh_count);

/* status code -> the BindStatus member name ("NONE".."EXPIRED"), else null. */
json_t *w24_onboard_d18acb_status_name(const json_t *status);

/* Onboarding constants and the BindStatus table, as an object. */
json_t *w24_onboard_d18acb_constants(void);

#ifdef __cplusplus
}
#endif
#endif
