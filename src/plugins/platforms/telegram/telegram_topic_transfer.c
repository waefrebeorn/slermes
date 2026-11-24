/*
 * telegram_topic_transfer.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_topic_transfer.py
 * (gate w25_telegramto_67ae35).
 *
 * PoP: w25_telegramto_67ae35_transfer @ plugins/platforms/telegram/telegram_topic_transfer.py:apply_root_dm_topic_transfer
 * PoP: w25_telegramto_67ae35_prompt_inputs @ plugins/platforms/telegram/telegram_topic_transfer.py:apply_topic_prompt_inputs
 * PoP: w25_telegramto_67ae35_recovery @ plugins/platforms/telegram/telegram_topic_transfer.py:apply_topic_recovery_with_inputs
 *
 * The adapter, the awaited transfer hook, the dm-topic lookup and
 * resolve_channel_prompt are seams: the hook's verdict and the topic info
 * arrive as fixed inputs.  Ported here is the ordering that keeps a lobby
 * message out of a topic it must not enter -- transfer hook present, DM
 * source, not internal, no command, no gateway session key, sender authorized
 * -- and the source rewrite a fresh topic needs.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_telegramto_67ae35.h"

static char *tg_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

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

/* str(v). */
static char *tg_pystr(const json_t *v) {
    char buf[64];
    if (!v) return tg_dup("None");
    switch (v->type) {
    case JSON_NULL:
        return tg_dup("None");
    case JSON_BOOL:
        return tg_dup(v->bool_val ? "True" : "False");
    case JSON_STRING:
        return tg_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%g", d);
        return tg_dup(buf);
    }
    default:
        return tg_dup("");
    }
}

/* Explicit ``is False``: only a JSON false blocks. */
static int tg_is_false(const json_t *v) {
    return v && v->type == JSON_BOOL && !v->bool_val;
}

static json_t *tg_not_moved(void) {
    json_t *out = json_object();
    json_set(out, "moved", json_bool(false));
    json_set(out, "thread_id", json_null());
    json_set(out, "message_id", json_null());
    json_set(out, "reply_anchor_override", json_null());
    json_set(out, "prompt_applied", json_bool(false));
    return out;
}

static json_t *tg_moved_json(const json_t *request) {
    const json_t *decided = json_obj_get(request, "decided");
    json_t *out = json_object();
    int created = tg_truthy(json_obj_get(decided, "created"));
    char *thread_id = tg_pystr(json_obj_get(decided, "thread_id"));

    json_set(out, "moved", json_bool(true));
    json_set(out, "thread_id", json_string(thread_id ? thread_id : ""));
    free(thread_id);
    if (created) {
        /* the root DM lives outside the new topic: it cannot anchor the reply */
        json_set(out, "message_id", json_null());
        json_set(out, "reply_anchor_override", json_string(""));
    } else {
        const json_t *mid = json_obj_get(json_obj_get(request, "source"), "message_id");
        json_set(out, "message_id", mid ? json_copy(mid) : json_null());
        json_set(out, "reply_anchor_override", json_null());
    }
    json_set(out, "prompt_applied", json_bool(true));
    return out;
}

json_t *w25_telegramto_67ae35_transfer(const json_t *request) {
    const json_t *source = json_obj_get(request, "source");
    const json_t *decided;
    const json_t *metadata;
    char *session_key;

    if (!tg_truthy(json_obj_get(request, "has_transfer"))) return tg_not_moved();
    if (!source || source->type != JSON_OBJECT) return tg_not_moved();
    if (!(json_obj_get(source, "chat_type") && json_obj_get(source, "chat_type")->type == JSON_STRING &&
          json_obj_get(source, "chat_type")->str_val &&
          !strcmp(json_obj_get(source, "chat_type")->str_val, "dm")))
        return tg_not_moved();
    if (tg_truthy(json_obj_get(request, "internal"))) return tg_not_moved();
    if (tg_truthy(json_obj_get(request, "command"))) return tg_not_moved();

    /* str((event.metadata or {}).get("gateway_session_key") or "").strip() */
    metadata = json_obj_get(request, "metadata");
    {
        const json_t *ks = json_obj_get(metadata, "gateway_session_key");
        const char *b;
        const char *e;
        session_key = tg_truthy(ks) ? tg_pystr(ks) : tg_dup("");
        b = session_key;
        while (*b && isspace((unsigned char)*b)) b++;
        e = b + strlen(b);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        if (e > b) { free(session_key); return tg_not_moved(); }
        free(session_key);
    }

    /* adapter._is_sender_authorized(...) is False */
    if (tg_is_false(json_obj_get(request, "authorized"))) return tg_not_moved();

    decided = json_obj_get(request, "decided");
    if (!decided || decided->type != JSON_OBJECT) return tg_not_moved();
    if (json_obj_get(decided, "raise")) return tg_not_moved();
    return tg_moved_json(request);
}

json_t *w25_telegramto_67ae35_prompt_inputs(const json_t *request) {
    const json_t *topic = json_obj_get(request, "topic");
    const json_t *name = json_obj_get(topic, "name");
    const json_t *skill = json_obj_get(topic, "skill");
    const json_t *prompt = json_obj_get(request, "channel_prompt");
    json_t *out = json_object();

    json_set(out, "chat_topic", name ? json_copy(name) : json_null());
    json_set(out, "auto_skill", skill ? json_copy(skill) : json_null());
    json_set(out, "channel_prompt", prompt ? json_copy(prompt) : json_null());
    return out;
}

json_t *w25_telegramto_67ae35_recovery(const json_t *request) {
    const json_t *before = json_obj_get(request, "before_thread");
    const json_t *after = json_obj_get(request, "after_thread");
    json_t *out = json_object();
    int changed;

    if (!before || before->type == JSON_NULL)
        changed = after && after->type != JSON_NULL;
    else if (!after || after->type == JSON_NULL)
        changed = 1;
    else if (before->type == JSON_STRING && after->type == JSON_STRING)
        changed = strcmp(before->str_val ? before->str_val : "",
                         after->str_val ? after->str_val : "") != 0;
    else if (before->type == JSON_NUMBER && after->type == JSON_NUMBER)
        changed = before->num_val != after->num_val;
    else
        changed = 1;   /* differing types are unequal in Python too */

    json_set(out, "applied", json_bool(changed ? true : false));
    if (changed) {
        json_t *inputs = w25_telegramto_67ae35_prompt_inputs(request);
        json_set(out, "chat_topic", json_copy(json_obj_get(inputs, "chat_topic")));
        json_free(inputs);
    } else {
        json_set(out, "chat_topic", json_null());
    }
    return out;
}
