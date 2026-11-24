/*
 * run_shutdown_home_notice.c - real C11 port of upstream Hermes
 *   gateway/run_shutdown_home_notice.py decision helpers: the broadcast skip
 *   reason, the loop mute, the active-session dedupe and the online-notice debt.
 * PoP: w19_runshutdow_75cb3d_broadcast_skip_reason @ gateway/run_shutdown_home_notice.py:_home_shutdown_broadcast_skip_reason
 * PoP: w19_runshutdow_75cb3d_notice_muted_by_loop @ gateway/run_shutdown_home_notice.py:_home_shutdown_notice_muted_by_loop
 * PoP: w19_runshutdow_75cb3d_got_active_session_notice @ gateway/run_shutdown_home_notice.py:_home_got_active_session_notice
 * PoP: w19_runshutdow_75cb3d_settle_notice_debt @ gateway/run_shutdown_home_notice.py:settle_home_online_notice_debt
 */
#include "noc_w19_runshutdow_75cb3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Outcome of this stop's home-channel broadcast, read by _stop_persist_exit_state. */
static const char *const HOME_NOTICE_SENT = "sent";
static const char *const HOME_NOTICE_MUTED = "muted";

static int jobj_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Python str() of a JSON scalar, into a caller buffer. */
static void scalar_str(const json_t *v, char *buf, size_t cap) {
    buf[0] = '\0';
    if (!v) return;
    if (v->type == JSON_STRING) {
        snprintf(buf, cap, "%s", v->str_val ? v->str_val : "");
    } else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, cap, "%lld", (long long)d);
        else snprintf(buf, cap, "%g", d);
    } else if (v->type == JSON_BOOL) {
        snprintf(buf, cap, "%s", v->bool_val ? "True" : "False");
    } else if (v->type == JSON_NULL) {
        snprintf(buf, cap, "None");
    }
}

static int looks_like_telegram_private_chat_id(const char *chat_id) {
    if (!chat_id || !*chat_id) return 0;
    char *end = NULL;
    long long v = strtoll(chat_id, &end, 10);
    if (end == chat_id || *end) return 0;
    return v > 0;
}

/* run_shutdown._delivery_target_key, flattened to one comparable string. */
static void delivery_key(const char *platform, const json_t *chat_id,
                         const json_t *thread_id, const char *profile,
                         char *out, size_t cap) {
    char chat[64], thread[64];
    scalar_str(chat_id, chat, sizeof(chat));
    int thread_truthy = jobj_truthy(thread_id);
    if (thread_truthy) scalar_str(thread_id, thread, sizeof(thread));
    else snprintf(thread, sizeof(thread), "None");
    char plat[128];
    snprintf(plat, sizeof(plat), "%s", platform ? platform : "");
    if (profile && profile[0] && strcmp(profile, "default") != 0
            && platform && !strcmp(platform, "telegram")
            && looks_like_telegram_private_chat_id(chat)) {
        snprintf(plat, sizeof(plat), "%s:%s", profile, platform);
    }
    snprintf(out, cap, "%s\x01%s\x01%s", plat, chat, thread);
}

static const char *obj_str(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

json_t *w19_runshutdow_75cb3d_broadcast_skip_reason(int restart_requested,
                                                   const char *restart_command_source,
                                                   int planned_takeover, int drain_suppressed,
                                                   int loop_tripped) {
    json_t *out = json_object();
    const char *skip = NULL;
    const char *notice = NULL;
    int loop_checked = 0;
    if (restart_requested && restart_command_source != NULL) {
        skip = "in-chat restart";
    } else if (planned_takeover) {
        skip = "planned --replace takeover";
    } else if (drain_suppressed) {
        skip = "drain marker (suppress_notification=true)";
    } else {
        /* _home_shutdown_notice_muted_by_loop always records the check. */
        loop_checked = 1;
        if (loop_tripped) {
            notice = HOME_NOTICE_MUTED;
            skip = "restart loop (gateway.restart_loop_guard)";
        }
    }
    json_set(out, "skip_reason", skip ? json_string(skip) : json_null());
    json_set(out, "notice", notice ? json_string(notice) : json_null());
    json_set(out, "loop_checked", json_bool(loop_checked));
    return out;
}

json_t *w19_runshutdow_75cb3d_notice_muted_by_loop(int loop_tripped) {
    json_t *out = json_object();
    json_set(out, "muted", json_bool(loop_tripped));
    json_set(out, "loop_checked", json_bool(1));
    return out;
}

json_t *w19_runshutdow_75cb3d_got_active_session_notice(json_t *configs, json_t *delivered) {
    if (!configs || configs->type != JSON_ARRAY) return json_bool(0);
    for (size_t i = 0; i < json_len(configs); i++) {
        json_t *cfg = json_get(configs, i);
        if (!cfg || cfg->type != JSON_OBJECT) continue;
        json_t *chat_id = json_obj_get(cfg, "chat_id");
        if (!jobj_truthy(chat_id)) continue;
        char want[256];
        delivery_key(obj_str(cfg, "platform"), chat_id, json_obj_get(cfg, "thread_id"),
                     obj_str(cfg, "profile"), want, sizeof(want));
        if (!delivered || delivered->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < json_len(delivered); j++) {
            json_t *d = json_get(delivered, j);
            if (!d || d->type != JSON_OBJECT) continue;
            char have[256];
            delivery_key(obj_str(d, "platform"), json_obj_get(d, "chat_id"),
                         json_obj_get(d, "thread_id"), obj_str(d, "profile"),
                         have, sizeof(have));
            if (!strcmp(want, have)) return json_bool(1);
        }
    }
    return json_bool(0);
}

json_t *w19_runshutdow_75cb3d_settle_notice_debt(const char *notice, int restart_requested,
                                                 int loop_checked) {
    json_t *out = json_object();
    int muted = notice && !strcmp(notice, HOME_NOTICE_MUTED);
    int sent = notice && !strcmp(notice, HOME_NOTICE_SENT);
    json_set(out, "drop_marker", json_bool(muted));
    /* The loop-muted stop also deletes a marker the CLI stamped; a delivered goodbye
     * writes one, so the next boot's "online" matches it. */
    json_set(out, "write_marker", json_bool(sent && !restart_requested));
    json_set(out, "record_delivered", json_bool(sent && loop_checked));
    json_set(out, "loop_checked", json_bool(0));
    return out;
}
