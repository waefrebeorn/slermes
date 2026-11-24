/*
 * callback_answer.c - C11 port of upstream
 * plugins/platforms/telegram/callback_answer.py (gate w25_callbackan_bbb650).
 *
 * PoP: w25_callbackan_bbb650_is_stale_query_error @ plugins/platforms/telegram/callback_answer.py:is_stale_query_error
 * PoP: w25_callbackan_bbb650_answer_callback @ plugins/platforms/telegram/callback_answer.py:answer_callback
 * PoP: w25_callbackan_bbb650_answer_or_note @ plugins/platforms/telegram/callback_answer.py:answer_or_note
 * PoP: w25_callbackan_bbb650_refuse_tap @ plugins/platforms/telegram/callback_answer.py:refuse_tap
 * PoP: w25_callbackan_bbb650_constants @ plugins/platforms/telegram/callback_answer.py:<module constants>
 *
 * Awaiting the query, PTB's exception classes and the adapter's control send are
 * seams. What is ported is the decision: which answer call is made, whether a
 * raised error is the expired-query refusal (and so must not escape), and where
 * a refused tap's fallback note lands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_callbackan_bbb650.h"

#define CB_TOAST_LIMIT W25_CALLBACKAN_BBB650_TOAST_LIMIT

/* _STALE_QUERY_PHRASES */
static const char *const CB_STALE_PHRASES[W25_CALLBACKAN_BBB650_STALE_PHRASE_COUNT] = {
    "query is too old",
    "query id is invalid",
    "response timeout expired",
};

/* Python truthiness of a JSON value. */
static int cb_truthy(const json_t *v) {
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

/* Python str() over the shapes a chat/thread id can hold. */
static void cb_str(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(out, cap, "%.0f", v->num_val);
        else snprintf(out, cap, "%.17g", v->num_val);
        return;
    }
    if (v->type == JSON_STRING) { snprintf(out, cap, "%s", v->str_val ? v->str_val : ""); return; }
    snprintf(out, cap, "%s", "");
}

/* str(exc).lower(): ASCII lowercase, in place into a fresh buffer. */
static char *cb_lower(const char *s) {
    size_t i, n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    out[n] = '\0';
    return out;
}

/* True when the wording carries one of the expired-query phrases. */
static int cb_phrase_match(const json_t *req) {
    const char *message = req ? json_get_str(req, "message", "") : "";
    char *lower = cb_lower(message);
    int hit = 0;
    size_t i;
    if (!lower) return 0;
    for (i = 0; i < W25_CALLBACKAN_BBB650_STALE_PHRASE_COUNT; i++)
        if (strstr(lower, CB_STALE_PHRASES[i])) { hit = 1; break; }
    free(lower);
    return hit;
}

json_t *w25_callbackan_bbb650_is_stale_query_error(const json_t *req) {
    if (!cb_phrase_match(req)) return json_bool(0);
    /* Without PTB importable the wording alone decides; with it, the error must
     * also be a BadRequest so an unrelated failure carrying the text stays loud. */
    if (!cb_truthy(req ? json_obj_get(req, "badrequest_importable") : NULL)) return json_bool(1);
    return json_bool(!strcmp(req ? json_get_str(req, "exc_type", "") : "", "BadRequest"));
}

/* query.answer(...) as one recorded call: {} for the bare call, else text and
 * show_alert only when it is True. */
static json_t *cb_answer_call(const json_t *text, int show_alert) {
    json_t *call = json_object();
    if (!text || text->type == JSON_NULL) return call;
    json_set(call, "text", json_copy(text));
    if (show_alert) json_set(call, "show_alert", json_bool(1));
    return call;
}

/* The answer_callback body. Returns the answer object; *raised tells the caller
 * whether the error escaped (a non-stale failure must stay visible). */
static json_t *cb_answer(const json_t *req, const json_t *text, int show_alert, int *raised) {
    const json_t *fault = req ? json_obj_get(req, "fault") : NULL;
    json_t *o = json_object(), *calls = json_array();

    *raised = 0;
    json_append(calls, cb_answer_call(text, show_alert));
    json_set(o, "calls", calls);
    if (fault && fault->type == JSON_OBJECT) {
        if (cb_truthy(w25_callbackan_bbb650_is_stale_query_error(fault)) == 0) {
            /* not a stale refusal: the error is re-raised */
            json_set(o, "result", json_null());
            json_set(o, "raised", json_bool(1));
            *raised = 1;
            return o;
        }
        json_set(o, "result", json_bool(0));
        json_set(o, "raised", json_bool(0));
        return o;
    }
    json_set(o, "result", json_bool(1));
    json_set(o, "raised", json_bool(0));
    return o;
}

json_t *w25_callbackan_bbb650_answer_callback(const json_t *req) {
    const json_t *text = req ? json_obj_get(req, "text") : NULL;
    int show_alert = cb_truthy(req ? json_obj_get(req, "show_alert") : NULL);
    int raised;
    return cb_answer(req, text, show_alert, &raised);
}

json_t *w25_callbackan_bbb650_answer_or_note(const json_t *req) {
    const json_t *text = req ? json_obj_get(req, "text") : NULL;
    const json_t *chat_id = req ? json_obj_get(req, "chat_id") : NULL;
    const json_t *thread_id = req ? json_obj_get(req, "message_thread_id") : NULL;
    const json_t *is_topic = req ? json_obj_get(req, "is_topic_message") : NULL;
    int raised, in_topic, answered;
    json_t *o = cb_answer(req, text, 0, &raised);
    answered = cb_truthy(json_obj_get(o, "result"));
    json_obj_del(o, "result");          /* answer_or_note returns None, not a result */

    if (raised) {                       /* answer_callback raised through */
        json_set(o, "sent", json_null());
        return o;
    }
    if (answered || !chat_id || chat_id->type == JSON_NULL) {
        json_set(o, "sent", json_null());
        return o;
    }
    in_topic = thread_id && thread_id->type != JSON_NULL
               && is_topic && is_topic->type == JSON_BOOL && is_topic->bool_val;
    if (cb_truthy(req ? json_obj_get(req, "send_fault") : NULL)) {
        json_set(o, "sent", json_null());
        return o;                       /* the note could not be sent: swallowed */
    }
    {
        json_t *sent = json_object();
        char buf[64];
        cb_str(chat_id, buf, sizeof(buf));
        json_set(sent, "chat_id", json_string(buf));
        json_set(sent, "text", text ? json_copy(text) : json_null());
        json_set(sent, "parse_mode", json_null());
        if (in_topic) {
            cb_str(thread_id, buf, sizeof(buf));
            json_set(sent, "thread_id", json_string(buf));
        } else {
            json_set(sent, "thread_id", json_null());
        }
        json_set(sent, "metadata", json_null());
        json_set(o, "sent", sent);
    }
    return o;
}

json_t *w25_callbackan_bbb650_refuse_tap(const json_t *req) {
    const json_t *requester = req ? json_obj_get(req, "requester") : NULL;
    json_t *o = json_object(), *calls = json_array();
    if (!cb_truthy(requester)) {
        json_set(o, "calls", calls);
        json_set(o, "result", json_bool(0));
        return o;
    }
    {
        /* refusal_text(requester)[:TOAST_LIMIT] */
        const char *refusal = req ? json_get_str(req, "refusal", "") : "";
        char sliced[CB_TOAST_LIMIT + 1];
        size_t n = strlen(refusal);
        if (n > CB_TOAST_LIMIT) n = CB_TOAST_LIMIT;
        memcpy(sliced, refusal, n);
        sliced[n] = '\0';
        json_append(calls, cb_answer_call(json_string(sliced), 1));
        json_set(o, "calls", calls);
        json_set(o, "result", json_bool(1));
    }
    return o;
}

json_t *w25_callbackan_bbb650_constants(void) {
    json_t *o = json_object(), *phrases = json_array();
    size_t i;
    json_set(o, "toast_limit", json_number((double)CB_TOAST_LIMIT));
    for (i = 0; i < W25_CALLBACKAN_BBB650_STALE_PHRASE_COUNT; i++)
        json_append(phrases, json_string(CB_STALE_PHRASES[i]));
    json_set(o, "stale_query_phrases", phrases);
    return o;
}
