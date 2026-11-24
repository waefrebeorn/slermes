/*
 * telegram_clarify_binding.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_clarify_binding.py
 * (gate w25_telegramcl_80d613).
 *
 * PoP: w25_telegramcl_80d613_ident @ plugins/platforms/telegram/telegram_clarify_binding.py:_ident
 * PoP: w25_telegramcl_80d613_from_sent @ plugins/platforms/telegram/telegram_clarify_binding.py:ClarifyPrompt.from_sent
 * PoP: w25_telegramcl_80d613_tap_matches @ plugins/platforms/telegram/telegram_clarify_binding.py:tap_matches_prompt
 * PoP: w25_telegramcl_80d613_audit_line @ plugins/platforms/telegram/telegram_clarify_binding.py:log_resolved
 *
 * The Bot API query/message objects and the logger are seams: their ids are
 * fixed inputs here.  What is ported is the comparison that decides whether a
 * cl:<id>:<idx> tap came from the prompt's own message, and the audit text.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_telegramcl_80d613.h"

static char *tg_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python str() of the scalar shapes a Telegram id can hold; None -> "None". */
static char *tg_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return tg_dup("None");
    switch (v->type) {
    case JSON_NULL:
        break;
    case JSON_BOOL:
        return tg_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val < 1e15 && v->num_val > -1e15)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return tg_dup(buf);
    case JSON_STRING:
        return tg_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:
        return tg_dup("[]");
    case JSON_OBJECT:
        return tg_dup("{}");
    }
    return tg_dup("None");
}

/* _ident(value): None stays None, everything else is str(value). */
static json_t *tg_ident_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    {
        char *s = tg_str(v);
        json_t *out;
        if (!s) return json_null();
        out = json_string(s);
        free(s);
        return out;
    }
}

/* Python (a == b) over two Optional[str] ids (NULL pointer == None). */
static int tg_opt_eq(const json_t *a, const json_t *b) {
    int a_none = !a || a->type == JSON_NULL;
    int b_none = !b || b->type == JSON_NULL;
    if (a_none || b_none) return a_none && b_none;
    return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
}

json_t *w25_telegramcl_80d613_ident(const json_t *req) {
    return tg_ident_value(json_obj_get(req, "value"));
}

json_t *w25_telegramcl_80d613_from_sent(const json_t *req) {
    const json_t *msg = json_obj_get(req, "msg");
    json_t *out = json_object();
    const char *session_key = json_get_str(req, "session_key", "");

    if (msg && msg->type != JSON_OBJECT) msg = NULL;
    json_set(out, "session_key", json_string(session_key));
    json_set(out, "chat_id", tg_ident_value(msg ? json_obj_get(msg, "chat_id") : NULL));
    json_set(out, "message_id", tg_ident_value(msg ? json_obj_get(msg, "message_id") : NULL));
    json_set(out, "thread_id",
             tg_ident_value(msg ? json_obj_get(msg, "message_thread_id") : NULL));
    return out;
}

json_t *w25_telegramcl_80d613_tap_matches(const json_t *req) {
    const json_t *prompt = json_obj_get(req, "prompt");
    const json_t *query = json_obj_get(req, "query");
    const json_t *message;
    const json_t *prompt_msg_id, *prompt_chat_id;
    const char *denial = json_get_str(req, "denial", "");
    json_t *got_chat, *got_msg, *out;
    int matched;

    if (prompt && prompt->type != JSON_OBJECT) prompt = NULL;
    if (query && query->type != JSON_OBJECT) query = NULL;
    prompt_msg_id = prompt ? json_obj_get(prompt, "message_id") : NULL;

    out = json_object();
    if (!prompt_msg_id || prompt_msg_id->type == JSON_NULL) {
        /* No bound message: resolve rather than strand the prompt. */
        json_set(out, "matched", json_bool(true));
        json_set(out, "answered", json_bool(false));
        json_set(out, "denial", json_null());
        return out;
    }
    message = query ? json_obj_get(query, "message") : NULL;
    if (message && message->type != JSON_OBJECT) message = NULL;
    got_chat = tg_ident_value(message ? json_obj_get(message, "chat_id") : NULL);
    got_msg = tg_ident_value(message ? json_obj_get(message, "message_id") : NULL);
    prompt_chat_id = json_obj_get(prompt, "chat_id");
    matched = tg_opt_eq(prompt_chat_id, got_chat) && tg_opt_eq(prompt_msg_id, got_msg);
    json_set(out, "matched", json_bool(matched ? true : false));
    json_set(out, "answered", json_bool(matched ? false : true));
    json_set(out, "denial", matched ? json_null() : json_string(denial));
    json_free(got_chat);
    json_free(got_msg);
    return out;
}

json_t *w25_telegramcl_80d613_audit_line(const json_t *req) {
    const json_t *choice = json_obj_get(req, "choice");
    char *clarify_id, *caller_id, *query_id, *chat, *message;
    size_t choice_len = 0, need;
    char *buf;
    json_t *out;

    if (choice && choice->type == JSON_STRING && choice->str_val)
        choice_len = strlen(choice->str_val);
    clarify_id = tg_str(json_obj_get(req, "clarify_id"));
    caller_id = tg_str(json_obj_get(req, "caller_id"));
    query_id = tg_str(json_obj_get(req, "query_id"));
    chat = tg_str(json_obj_get(req, "chat"));
    message = tg_str(json_obj_get(req, "message"));
    if (!clarify_id || !caller_id || !query_id || !chat || !message) {
        free(clarify_id); free(caller_id); free(query_id); free(chat); free(message);
        return json_null();
    }
    need = strlen(clarify_id) + strlen(caller_id) + strlen(query_id)
           + strlen(chat) + strlen(message) + 160;
    buf = (char *)malloc(need);
    if (!buf) {
        free(clarify_id); free(caller_id); free(query_id); free(chat); free(message);
        return json_null();
    }
    snprintf(buf, need,
             "Telegram clarify button resolved (id=%s, choice_len=%zu, caller_id=%s, "
             "callback_query=%s, chat=%s, message=%s)",
             clarify_id, choice_len, caller_id, query_id, chat, message);
    out = json_string(buf);
    free(clarify_id); free(caller_id); free(query_id); free(chat); free(message);
    free(buf);
    return out;
}
