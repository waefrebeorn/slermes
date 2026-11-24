/*
 * telegram_dm_topic_send.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_dm_topic_send.py
 * (gate w24_telegramdm_1f8bc2).
 *
 * PoP: w24_telegramdm_1f8bc2_private_chat @ plugins/platforms/telegram/telegram_dm_topic_send.py:private_chat
 * PoP: w24_telegramdm_1f8bc2_topic_gone @ plugins/platforms/telegram/telegram_dm_topic_send.py:topic_gone
 * PoP: w24_telegramdm_1f8bc2_dead_topic_may_reroute @ plugins/platforms/telegram/telegram_dm_topic_send.py:dead_topic_may_reroute
 * PoP: w24_telegramdm_1f8bc2_dead_topic_retry_kwargs @ plugins/platforms/telegram/telegram_dm_topic_send.py:dead_topic_retry_kwargs
 * PoP: w24_telegramdm_1f8bc2_constants @ plugins/platforms/telegram/telegram_dm_topic_send.py:<marker tuples>
 *
 * Ported: the whole send ladder except the Bot API call itself.  What is left
 * is a pure function of values the caller already holds -- the chat id, the
 * error the API raised, the routing metadata and the kwargs of the failed
 * send.  The seam is the send/repo/prune side: bot.send_message,
 * RoutingRepository.delete_topic_binding and the DeliveryRouter recreate are
 * never invoked; only what those values decide is reproduced here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegramdm_1f8bc2.h"

/* Wordings a private-chat topic that no longer takes messages answers with. */
static const char *const TD_PRIVATE_MARKERS[W24_TELEGRAMDM_1F8BC2_PRIVATE_MARKERS] = {
    "thread not found", "topic_deleted", "topic not found", "direct_messages_topic",
};
/* Elsewhere only this one says anything about the topic. */
static const char *const TD_PUBLIC_MARKERS[W24_TELEGRAMDM_1F8BC2_PUBLIC_MARKERS] = {
    "thread not found",
};
static const char *const TD_DROPPED_ROUTE_KEYS[2] = {
    "message_thread_id", "direct_messages_topic_id",
};

#define TD_MAX_TEXT 4096

static int td_truthy(const json_t *v) {
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

/* str(v): the text Python's str() hands to .strip() / .lower() for the shapes
 * a chat id, an exception message or a header value can carry. */
static void td_text(const json_t *v, char *buf, size_t cap) {
    if (cap == 0) return;
    buf[0] = '\0';
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
    case JSON_STRING:
        snprintf(buf, cap, "%s", v->str_val ? v->str_val : "");
        return;
    case JSON_BOOL:
        snprintf(buf, cap, "%s", v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val > -1e16 && v->num_val < 1e16)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%.17g", v->num_val);
        return;
    default: {
        char *s = json_dumps(v, 0);
        snprintf(buf, cap, "%s", s ? s : "");
        free(s);
        return;
    }
    }
}

static int td_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* int(text) > 0; False for every text Python's int() rejects (ValueError) and
 * for anything int() cannot take at all (TypeError, folded into td_text). */
static int td_int_positive(const char *s) {
    size_t i = 0, n;
    int neg = 0, digit_seen = 0, nonzero = 0, prev_digit = 0;
    if (!s) return 0;
    n = strlen(s);
    while (i < n && td_is_space(s[i])) i++;
    if (i < n && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
    for (; i < n; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            digit_seen = 1;
            prev_digit = 1;
            if (c != '0') nonzero = 1;
            continue;
        }
        if (c == '_' && prev_digit && i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '9') {
            prev_digit = 0;   /* an underscore needs a digit on both sides */
            continue;
        }
        if (td_is_space(c)) {
            while (i < n && td_is_space(s[i])) i++;
            break;
        }
        return 0;             /* sign-only, bare underscore, trailing garbage */
    }
    if (i < n) return 0;
    if (!digit_seen || !prev_digit) return 0;
    if (!nonzero) return 0;   /* 0 and -0 are not > 0 */
    return neg ? 0 : 1;
}

json_t *w24_telegramdm_1f8bc2_private_chat(const json_t *chat_id) {
    char buf[TD_MAX_TEXT];
    td_text(chat_id, buf, sizeof(buf));
    return json_bool(td_int_positive(buf) ? true : false);
}

json_t *w24_telegramdm_1f8bc2_topic_gone(const json_t *chat_id, const json_t *error) {
    char idbuf[TD_MAX_TEXT];
    char ebuf[TD_MAX_TEXT];
    const char *const *markers;
    size_t count, i;
    int is_private;

    td_text(chat_id, idbuf, sizeof(idbuf));
    is_private = td_int_positive(idbuf);
    markers = is_private ? TD_PRIVATE_MARKERS : TD_PUBLIC_MARKERS;
    count = is_private ? W24_TELEGRAMDM_1F8BC2_PRIVATE_MARKERS
                       : W24_TELEGRAMDM_1F8BC2_PUBLIC_MARKERS;

    td_text(error, ebuf, sizeof(ebuf));
    for (i = 0; ebuf[i]; i++)
        if (ebuf[i] >= 'A' && ebuf[i] <= 'Z') ebuf[i] = (char)(ebuf[i] - 'A' + 'a');
    for (i = 0; i < count; i++)
        if (strstr(ebuf, markers[i])) return json_bool(true);
    return json_bool(false);
}

json_t *w24_telegramdm_1f8bc2_dead_topic_may_reroute(const json_t *chat_id,
                                                     const json_t *metadata) {
    char buf[TD_MAX_TEXT];
    const json_t *flag;

    td_text(chat_id, buf, sizeof(buf));
    if (!td_int_positive(buf)) return json_bool(false);
    /* (metadata or {}).get(...): a falsy metadata behaves as an empty mapping. */
    flag = (metadata && metadata->type == JSON_OBJECT)
               ? json_obj_get(metadata, "telegram_dm_topic_created_for_send")
               : NULL;
    return json_bool(td_truthy(flag) ? false : true);
}

json_t *w24_telegramdm_1f8bc2_dead_topic_retry_kwargs(const json_t *kwargs) {
    json_t *retry;
    char buf[TD_MAX_TEXT];
    size_t i;

    retry = (kwargs && kwargs->type == JSON_OBJECT) ? json_copy(kwargs) : json_object();
    if (!retry || retry->type != JSON_OBJECT) {
        json_free(retry);
        return json_object();
    }
    for (i = 0; i < sizeof(TD_DROPPED_ROUTE_KEYS) / sizeof(TD_DROPPED_ROUTE_KEYS[0]); i++)
        json_obj_del(retry, TD_DROPPED_ROUTE_KEYS[i]);
    td_text(json_obj_get(kwargs, "chat_id"), buf, sizeof(buf));
    if (td_int_positive(buf)) json_obj_del(retry, "reply_to_message_id");
    return retry;
}

