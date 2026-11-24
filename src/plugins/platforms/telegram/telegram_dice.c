/*
 * telegram_dice.c -- real C11 port of plugins/platforms/telegram/telegram_dice.py
 * (gate w22_telegramdi_2d8019).
 *
 * PoP: w22_telegramdi_2d8019_event_text @ plugins/platforms/telegram/telegram_dice.py:handle_dice_message
 * PoP: w22_telegramdi_2d8019_handle @ plugins/platforms/telegram/telegram_dice.py:handle_dice_message
 *
 * Same gate/observe path as a contact card: an observed throw keeps its roll,
 * an authorized mention is handed on with group attribution applied. The adapter
 * is a PTB object (network seam); every decision it feeds is reproduced here.
 */
#include "hermes_json.h"
#include "noc_w22_telegramdi_2d8019.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int td_2d8019_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* A malloc'd copy of a literal; NULL only when the allocation fails. */
static char *td_2d8019_lit(const char *lit) {
    size_t n = strlen(lit);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, lit, n + 1);
    return s;
}

/* str(value) for the scalar shapes an emoji/roll carries. */
static char *td_2d8019_str(const json_t *v) {
    char buf[64];
    if (!v) return NULL;
    switch (v->type) {
    case JSON_STRING: {
        size_t n = strlen(v->str_val ? v->str_val : "");
        char *s = (char *)malloc(n + 1);
        if (!s) return NULL;
        memcpy(s, v->str_val ? v->str_val : "", n + 1);
        return s;
    }
    case JSON_BOOL:
        return td_2d8019_lit(v->bool_val ? "True" : "False");
    case JSON_NULL:
        return td_2d8019_lit("None");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            v->num_val > -9.007199254740992e15 && v->num_val < 9.007199254740992e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return td_2d8019_lit(buf);
    default:
        return td_2d8019_lit("");
    }
}

static json_t *td_2d8019_raised(const char *type) {
    json_t *out = json_object();
    json_set(out, "raised", json_string(type));
    return out;
}

json_t *w22_telegramdi_2d8019_event_text(const json_t *emoji, const json_t *value) {
    static const char *const PRE = "[The user sent a Telegram dice: ";
    static const char *const MID = " rolled ";
    static const char *const POST = ".]";
    char *em = td_2d8019_str(emoji);
    char *val = td_2d8019_str(value);
    size_t n;
    char *text;
    json_t *out;

    if (!em) em = strcpy((char *)malloc(5), "None");
    if (!val) val = strcpy((char *)malloc(5), "None");
    if (!em || !val) { free(em); free(val); return json_string(""); }

    n = strlen(PRE) + strlen(em) + strlen(MID) + strlen(val) + strlen(POST) + 1;
    text = (char *)malloc(n);
    if (!text) { free(em); free(val); return json_string(""); }
    snprintf(text, n, "%s%s%s%s%s", PRE, em, MID, val, POST);
    out = json_string(text);
    free(text);
    free(em);
    free(val);
    return out;
}

json_t *w22_telegramdi_2d8019_handle(const json_t *in) {
    json_t *msg, *dice, *calls, *out, *event;
    json_t *emoji, *value, *uid;
    int is_object_message, proceed, observe;

    if (!in || in->type != JSON_OBJECT) in = NULL;
    msg = json_obj_get(in, "msg");
    /* `if msg` -- the message is an adapter object; anything that is not an
     * object (null, false, 0, "", a scalar) has no `dice` attribute either. */
    is_object_message = msg && msg->type == JSON_OBJECT;

    calls = json_array();
    out = json_object();
    json_set(out, "event", json_null());

    dice = is_object_message ? json_obj_get(in, "dice") : NULL;
    if (!dice || dice->type == JSON_NULL) {
        /* `getattr(msg, "dice", None)` is None: the throw vanishes silently. */
        json_set(out, "calls", calls);
        return out;
    }

    if (!td_2d8019_truthy(json_obj_get(in, "authorized"))) {
        json_append(calls, json_string("log_blocked_user"));
        json_set(out, "calls", calls);
        return out;
    }

    proceed = td_2d8019_truthy(json_obj_get(in, "proceed"));
    observe = td_2d8019_truthy(json_obj_get(in, "observe"));
    if (!proceed && !observe) {
        json_set(out, "calls", calls);
        return out;
    }

    uid = json_obj_get(in, "update_id");
    if (!uid) {
        json_free(calls);
        json_free(out);
        return td_2d8019_raised("AttributeError");   /* update.update_id absent */
    }
    emoji = json_obj_get(dice, "emoji");
    value = json_obj_get(dice, "value");
    if (!emoji || !value) {
        json_free(calls);
        json_free(out);
        return td_2d8019_raised("AttributeError");   /* dice.emoji / dice.value */
    }

    event = json_object();
    json_set(event, "kind", json_string("TEXT"));
    json_set(event, "update_id", json_copy(uid));
    json_set(event, "text", w22_telegramdi_2d8019_event_text(emoji, value));
    json_append(calls, json_string("build_message_event"));

    if (!proceed) {
        /* Observed chatter keeps the roll; the message has no text to save. */
        json_set(event, "attributed", json_bool(0));
        json_append(calls, json_string("observe"));
        json_set(out, "event", event);
        json_set(out, "calls", calls);
        return out;
    }

    json_set(event, "attributed", json_bool(1));
    json_append(calls, json_string("apply_attribution"));
    json_append(calls, json_string("handle_message"));
    json_set(out, "event", event);
    json_set(out, "calls", calls);
    return out;
}

