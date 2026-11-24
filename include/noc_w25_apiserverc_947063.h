/*
 * noc_w25_apiserverc_947063.h - C surface for upstream
 * gateway/platforms/api_server_cron_jobs.py (gate w25_apiserverc_947063).
 *
 * The PATCH /api/jobs/{job_id} decision core: the whitelist a request body is
 * filtered through, the self-judgeable 400s (empty update, non-string field,
 * name cap, prompt cap) and the job_response status selection (404/400/500/200
 * plus the notify call).  aiohttp, the store callables and the prompt scanner
 * are seams: fixed values go in, the HTTP decision comes out.
 */
#ifndef NOC_W25_APISERVERC_947063_H
#define NOC_W25_APISERVERC_947063_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* adapter._UPDATE_ALLOWED_FIELDS as a sorted array of names. */
json_t *w25_apiserverc_947063_allowed_fields(void);

/* {"body": <any>, "allowed": [<str>]?} -> the whitelisted body object, or null
 * when the body is not a JSON object (the handler's 400 guard). */
json_t *w25_apiserverc_947063_sanitize(const json_t *req);

/* {"sanitized": <any>, "max_name_length": <int>?, "prompt_error": <str>?} ->
 * null when the PATCH value passes, else {"status":400,"error":<text>}. */
json_t *w25_apiserverc_947063_invalid_update(const json_t *req);

/* {"job": <any>, "notify": <bool>, "error": {"kind","message"}|null} ->
 * {"status":<int>,"body":<obj>,"notified":<bool>}. */
json_t *w25_apiserverc_947063_job_status(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
