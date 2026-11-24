/*
 * telegram_media_retry.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_media_retry.py
 * (gate w25_telegramme_bdc44d).
 *
 * PoP: w25_telegramme_bdc44d_request_may_have_delivered @ plugins/platforms/telegram/telegram_media_retry.py:request_may_have_delivered
 * PoP: w25_telegramme_bdc44d_unconfirmed_media_result @ plugins/platforms/telegram/telegram_media_retry.py:unconfirmed_media_result
 * PoP: w25_telegramme_bdc44d_retry_plan @ plugins/platforms/telegram/telegram_media_retry.py:send_media_retrying
 * PoP: w25_telegramme_bdc44d_constants @ plugins/platforms/telegram/telegram_media_retry.py:<module constants>
 *
 * The PTB exception graph, the socket and the upload are seams: the
 * classifier's verdict arrives as ``kind`` (one of the telegram_send_errors
 * members) and each attempt's outcome is a fixed input.  What is ported is the
 * decision those values make -- is the failure one Telegram provably never
 * posted (retry the SAME request, with the same budget, rewinding the upload),
 * or one that may already be in the chat (never resend it in any shape; report
 * it ambiguous, or uncertain-but-failed for a URL sender).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_telegramme_bdc44d.h"

/* telegram_send_errors: the kinds that never left / Telegram's own 5xx JSON. */
#define TG_UNSENT "unsent"
#define TG_SERVER_ERROR "server_error"
#define TG_LOST "lost"

static int tg_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static const char *tg_kind(const json_t *state) {
    const char *k = state && state->type == JSON_OBJECT ? json_get_str(state, "kind", "") : "";
    return k ? k : "";
}

static int tg_retryable(const char *kind) {
    return !strcmp(kind, TG_UNSENT) || !strcmp(kind, TG_SERVER_ERROR);
}

/* bool(getattr(error, _REQUEST_ERROR_ATTR, False)) and kind == LOST */
json_t *w25_telegramme_bdc44d_request_may_have_delivered(const json_t *state) {
    const json_t *flagged = state && state->type == JSON_OBJECT ? json_obj_get(state, "request_error") : NULL;
    int may = tg_truthy(flagged) && !strcmp(tg_kind(state), TG_LOST);
    return json_bool(may ? true : false);
}

static json_t *tg_result(int success, const char *error, int retryable, json_t *raw) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(success ? true : false));
    json_set(o, "message_id", json_null());
    json_set(o, "error", error ? json_string(error) : json_null());
    json_set(o, "retryable", json_bool(retryable ? true : false));
    json_set(o, "raw_response", raw ? raw : json_null());
    return o;
}

json_t *w25_telegramme_bdc44d_unconfirmed_media_result(const json_t *state) {
    json_t *may, *raw;
    const char *text;
    char safe_error[512];
    int as_failure = 0;

    may = w25_telegramme_bdc44d_request_may_have_delivered(state);
    if (!may || !tg_truthy(may)) { json_free(may); return json_null(); }
    json_free(may);

    /* _redact_telegram_error_text: the secret scrubber is a seam, but its
     * empty-text rule is not -- httpx timeouts stringify to "", and the class
     * name is kept so failure lines never log an empty reason. */
    text = state && state->type == JSON_OBJECT ? json_get_str(state, "error_text", "") : "";
    if (!text) text = "";
    if (!text[0]) {
        const char *type_name = json_get_str(state, "error_type", "");
        snprintf(safe_error, sizeof safe_error, "<%s>", (type_name && type_name[0]) ? type_name : "Exception");
    } else {
        snprintf(safe_error, sizeof safe_error, "%s", text);
    }
    as_failure = tg_truthy(json_obj_get(state, "as_failure"));

    if (as_failure) {
        raw = json_object();
        json_set(raw, "delivery_uncertain", json_bool(true));
        json_set(raw, "failure_notice_sent", json_bool(true));
        return tg_result(0, safe_error, 0, raw);
    }
    raw = json_object();
    json_set(raw, "ambiguous", json_bool(true));
    json_set(raw, "error", json_string(safe_error));
    return tg_result(1, NULL, 0, raw);
}

/* send_media_retrying: ``attempts`` are the outcomes send_once() would produce,
 * in order.  ``2 ** attempt`` backs off; the upload is rewound before a repeat. */
json_t *w25_telegramme_bdc44d_retry_plan(const json_t *state) {
    const json_t *attempts;
    json_t *out, *delays;
    int use_reset = 0;
    int attempt;
    size_t n;
    int raised = 0, calls = 0;

    attempts = state && state->type == JSON_OBJECT ? json_obj_get(state, "attempts") : NULL;
    n = (attempts && attempts->type == JSON_ARRAY) ? json_len(attempts) : 0;
    use_reset = tg_truthy(state && state->type == JSON_OBJECT ? json_obj_get(state, "reset") : NULL);

    delays = json_array();
    for (attempt = 0; attempt < W25_TELEGRAMME_BDC44D_SAME_REQUEST_ATTEMPTS; attempt++) {
        const json_t *a = (size_t)attempt < n ? json_get(attempts, (size_t)attempt) : NULL;
        int ok = tg_truthy(a && a->type == JSON_OBJECT ? json_obj_get(a, "ok") : NULL);
        const char *kind;
        calls++;
        if (ok) break;
        kind = a && a->type == JSON_OBJECT ? json_get_str(a, "kind", "") : "";
        if (!kind) kind = "";
        if (attempt >= W25_TELEGRAMME_BDC44D_SAME_REQUEST_ATTEMPTS - 1 || !tg_retryable(kind)) {
            raised = 1;
            break;
        }
        json_append(delays, json_number((double)(1 << attempt)));
        if (use_reset) { /* reset_media() rewinds the upload before the repeat */ }
    }

    out = json_object();
    json_set(out, "status", json_string(raised ? "raise" : "ok"));
    json_set(out, "attempts", json_number((double)calls));
    json_set(out, "delays", delays);
    {
        size_t i, m = json_len(delays), resets = 0;
        for (i = 0; i < m; i++) if (use_reset) resets++;
        json_set(out, "resets", json_number((double)resets));
    }
    json_set(out, "error_attr", json_bool(raised ? true : false));
    return out;
}

json_t *w25_telegramme_bdc44d_constants(void) {
    json_t *o = json_object();
    json_t *retryable = json_array();
    json_set(o, "same_request_attempts", json_number((double)W25_TELEGRAMME_BDC44D_SAME_REQUEST_ATTEMPTS));
    json_set(o, "request_error_attr", json_string(W25_TELEGRAMME_BDC44D_REQUEST_ERROR_ATTR));
    json_append(retryable, json_string(TG_UNSENT));
    json_append(retryable, json_string(TG_SERVER_ERROR));
    json_set(o, "retryable_kinds", retryable);
    json_set(o, "lost_kind", json_string(TG_LOST));
    return o;
}
