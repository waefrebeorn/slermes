/*
 * telegram_group_observe.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_group_observe.py
 * (gate w24_telegramgr_e8bd89).
 *
 * PoP: w24_telegramgr_e8bd89_attribution_plan @ plugins/platforms/telegram/telegram_group_observe.py:apply_group_observe_attribution
 * PoP: w24_telegramgr_e8bd89_observed_media_note @ plugins/platforms/telegram/telegram_group_observe.py:observed_media_note
 * PoP: w24_telegramgr_e8bd89_shared_sender_suffix @ plugins/platforms/telegram/telegram_group_observe.py:shared_sender_suffix
 * PoP: w24_telegramgr_e8bd89_constants @ plugins/platforms/telegram/telegram_group_observe.py:_OBSERVED_MEDIA_TOOL
 *
 * The adapter is a seam: the raw Telegram message, the allowed-chat cache, the
 * channel prompt, the attributed text and the shared-source builder all arrive
 * as plain values, so nothing here opens a socket or reads config. What is
 * ported is the decision those values carry -- when an observed group turn is
 * rewritten onto the chat's shared session, what the joined channel prompt is,
 * how the trusted sender is named in the batch key, and the media-tool table
 * that tells a later turn how to open a cached file on demand.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegramgr_e8bd89.h"

/* _OBSERVED_MEDIA_TOOL: video_analyze is opt-in, so its hint keeps a fallback
 * the default bundle has. */
static const char *const TGO_TOOL_IMAGE = "vision_analyze with this path as image_url";
static const char *const TGO_TOOL_VIDEO =
    "video_analyze with this path as video_url when that tool is available, "
    "otherwise your file tools";
static const char *const TGO_TOOL_DEFAULT = "your file tools on this path";

static const json_t *tgo_arg(const json_t *req, const char *key) {
    return req && req->type == JSON_OBJECT ? json_obj_get(req, key) : NULL;
}

/* Python truthiness of a JSON value. */
static int tgo_truthy(const json_t *v) {
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

/* str(value) the way an f-string renders it; None renders as "None". */
static char *tgo_text(const json_t *v) {
    char buf[64];
    const char *s = NULL;
    char *dup;
    if (!v || v->type == JSON_NULL) {
        dup = (char *)malloc(5);
        if (dup) memcpy(dup, "None", 5);
        return dup;
    }
    if (v->type == JSON_STRING) {
        s = v->str_val ? v->str_val : "";
    } else if (v->type == JSON_BOOL) {
        s = v->bool_val ? "True" : "False";
    } else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        s = buf;
    } else {
        return NULL;
    }
    dup = (char *)malloc(strlen(s) + 1);
    if (dup) memcpy(dup, s, strlen(s) + 1);
    return dup;
}

/* "<cp>\n\n<op>" when cp is truthy, else <op> unchanged. */
static json_t *tgo_join_prompt(const json_t *channel_prompt, const json_t *observe_prompt) {
    char *cp, *op, *buf;
    size_t need;
    json_t *out;

    if (!tgo_truthy(channel_prompt)) {
        if (!observe_prompt) return json_null();
        return json_copy(observe_prompt);
    }
    cp = tgo_text(channel_prompt);
    op = tgo_text(observe_prompt);
    if (!cp && !op) return json_null();
    need = (cp ? strlen(cp) : 0) + 2 + (op ? strlen(op) : 0) + 1;
    buf = (char *)malloc(need);
    if (!buf) { free(cp); free(op); return json_null(); }
    snprintf(buf, need, "%s\n\n%s", cp ? cp : "", op ? op : "");
    out = json_string(buf);
    free(buf);
    free(cp);
    free(op);
    return out;
}

/* adapter._chat_id_str(raw) in adapter._telegram_observe_allowed_chats() */
static int tgo_chat_allowed(const json_t *allowed, const json_t *chat_id) {
    size_t i, n;
    const char *want;
    if (!tgo_truthy(allowed)) return 0;
    if (!chat_id || chat_id->type != JSON_STRING || !chat_id->str_val) return 0;
    want = chat_id->str_val;
    if (allowed->type == JSON_STRING)
        return strcmp(allowed->str_val ? allowed->str_val : "", want) == 0;
    if (allowed->type != JSON_ARRAY) return 0;
    n = json_len(allowed);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(allowed, i);
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, want) == 0)
            return 1;
    }
    return 0;
}

static void tgo_set_copy(json_t *obj, const char *key, const json_t *v) {
    json_set(obj, key, v ? json_copy(v) : json_null());
}

json_t *w24_telegramgr_e8bd89_attribution_plan(const json_t *req) {
    const json_t *v, *text, *sid, *cp;
    int observe, has_raw, is_group, applied, command;
    json_t *out = json_object();

    observe = tgo_truthy(tgo_arg(req, "observe_enabled"));
    has_raw = tgo_truthy(tgo_arg(req, "has_raw"));
    is_group = tgo_truthy(tgo_arg(req, "is_group"));
    applied = observe && has_raw && is_group &&
              tgo_chat_allowed(tgo_arg(req, "allowed"), tgo_arg(req, "chat_id"));

    text = tgo_arg(req, "text");
    sid = tgo_arg(req, "source_user_id");
    cp = tgo_arg(req, "channel_prompt");

    json_set(out, "applied", json_bool(applied ? true : false));
    if (!applied) {
        json_set(out, "shared_session",
                 json_bool(tgo_truthy(tgo_arg(req, "source_shared_session")) ? true : false));
        tgo_set_copy(out, "text", text);
        tgo_set_copy(out, "channel_prompt", cp);
        tgo_set_copy(out, "source_user_id", sid);
        return out;
    }

    v = tgo_arg(req, "message_type");
    command = v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, "command") == 0;

    json_set(out, "channel_prompt", tgo_join_prompt(cp, tgo_arg(req, "observe_prompt")));
    if (command) {
        /* Commands keep user_id (slash access checks) but share the session. */
        json_set(out, "shared_session", json_bool(true));
        tgo_set_copy(out, "text", text);
        tgo_set_copy(out, "source_user_id", sid);
        return out;
    }
    /* Text turns get the sender-less shared source plus the [name|id] tag. */
    json_set(out, "shared_session",
             json_bool(tgo_truthy(tgo_arg(req, "shared_source_shared_session")) ? true : false));
    tgo_set_copy(out, "text", tgo_arg(req, "attributed_text"));
    tgo_set_copy(out, "source_user_id", tgo_arg(req, "shared_source_user_id"));
    return out;
}

json_t *w24_telegramgr_e8bd89_observed_media_note(const json_t *req) {
    const json_t *kind = tgo_arg(req, "kind");
    const char *how = TGO_TOOL_DEFAULT;
    char *note, *buf;
    size_t need;
    json_t *out;

    if (kind && kind->type == JSON_STRING && kind->str_val) {
        if (strcmp(kind->str_val, "image") == 0) how = TGO_TOOL_IMAGE;
        else if (strcmp(kind->str_val, "video") == 0) how = TGO_TOOL_VIDEO;
    }
    note = tgo_text(tgo_arg(req, "context_note"));
    if (!note) return json_null();
    need = strlen(note) + strlen(" (not attached inline; to look at it use )") +
           strlen(how) + 1;
    buf = (char *)malloc(need);
    if (!buf) { free(note); return json_null(); }
    snprintf(buf, need, "%s (not attached inline; to look at it use %s)", note, how);
    out = json_string(buf);
    free(buf);
    free(note);
    return out;
}

json_t *w24_telegramgr_e8bd89_shared_sender_suffix(const json_t *req) {
    const json_t *actor, *uid = tgo_arg(req, "user_id");
    char *name, *buf;
    size_t need;
    json_t *out;

    /* actor = getattr(event, "user_id", None) or event.source.user_id */
    actor = tgo_truthy(uid) ? uid : tgo_arg(req, "source_user_id");
    if (!tgo_truthy(actor)) return json_string("");
    name = tgo_text(actor);
    if (!name) return json_string("");
    need = strlen(":from:") + strlen(name) + 1;
    buf = (char *)malloc(need);
    if (!buf) { free(name); return json_string(""); }
    snprintf(buf, need, ":from:%s", name);
    out = json_string(buf);
    free(buf);
    free(name);
    return out;
}

json_t *w24_telegramgr_e8bd89_constants(void) {
    json_t *o = json_object();
    json_set(o, "image", json_string(TGO_TOOL_IMAGE));
    json_set(o, "video", json_string(TGO_TOOL_VIDEO));
    return o;
}
