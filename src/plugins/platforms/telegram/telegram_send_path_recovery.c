/*
 * telegram_send_path_recovery.c -- real C11 port of the Telegram in-place
 * send-path recovery edge in upstream Hermes
 * plugins/platforms/telegram/telegram_send_path_recovery.py.
 *
 * PoP: w20_telegramse_1853c7_finalize @ plugins/platforms/telegram/telegram_send_path_recovery.py:TelegramSendPathRecoveryMixin._finalize_delivery_obligation
 * PoP: w20_telegramse_1853c7_mark_recovered @ plugins/platforms/telegram/telegram_send_path_recovery.py:mark_send_path_recovered
 * PoP: w20_telegramse_1853c7_schedule_sweep @ plugins/platforms/telegram/telegram_send_path_recovery.py:_schedule_recovery_sweep
 * PoP: w20_telegramse_1853c7_sweep_loop @ plugins/platforms/telegram/telegram_send_path_recovery.py:_schedule_recovery_sweep
 *
 * The adapter, its gateway runner and the event loop are seams: each entry
 * point takes the scripted adapter and answers the ordered action trace the
 * Python edge would perform (clear / status write / task create / replay /
 * redeliver), so the decisions are comparable without asyncio or a socket.
 */
#include "hermes_json.h"
#include "noc_w20_telegramse_1853c7.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS_MAX_ITERS 32

static int ts_truthy(const json_t *v) {
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

static const json_t *ts_get(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

static int ts_flag(const json_t *adapter, const char *key, int def) {
    const json_t *v = ts_get(adapter, key);
    if (!v || v->type == JSON_NULL) return def;
    return ts_truthy(v);
}

static const char *ts_text(const json_t *adapter, const char *key, const char *def) {
    const json_t *v = ts_get(adapter, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static void ts_push(json_t *actions, const char *text) {
    json_append(actions, json_string(text));
}

static json_t *ts_result(json_t *actions) {
    json_t *o = json_object();
    json_set(o, "actions", actions);
    return o;
}

/* _schedule_recovery_sweep's decision, shared by the mark_recovered edge. */
static void ts_schedule(const json_t *adapter, json_t *actions, int *rerun_out,
                        char *task_out, size_t cap) {
    const char *name = ts_text(adapter, "name", "");
    if (!ts_flag(adapter, "has_redeliver", 1)) return;   /* not callable: silent return */
    if (ts_flag(adapter, "task_running", 0)) {
        if (rerun_out) *rerun_out = 1;
        return;
    }
    if (rerun_out) *rerun_out = 0;
    snprintf(task_out, cap, "telegram-send-path-recovery:%s", name);
    {
        char action[300];
        snprintf(action, sizeof action, "create:%s", task_out);
        ts_push(actions, action);
    }
}

json_t *w20_telegramse_1853c7_finalize(const json_t *result, const json_t *adapter) {
    json_t *actions = json_array();
    int success = ts_flag(result, "success", 0);
    const char *error = ts_text(result, "error", "");
    int is_mixin = ts_flag(adapter, "is_mixin", 1);
    int degraded = ts_flag(adapter, "degraded", 0);
    int running = ts_flag(adapter, "running", 0);

    ts_push(actions, "super");
    if (!success && !strcmp(error, "send_path_degraded") && is_mixin &&
        !degraded && running)
        ts_push(actions, "sweep");
    return ts_result(actions);
}

json_t *w20_telegramse_1853c7_schedule_sweep(const json_t *adapter) {
    json_t *actions = json_array();
    json_t *out = NULL;
    int rerun = ts_flag(adapter, "rerun", 0);
    char task[256];
    task[0] = '\0';

    ts_schedule(adapter, actions, &rerun, task, sizeof task);
    out = ts_result(actions);
    json_set(out, "degraded", json_bool(ts_flag(adapter, "degraded", 0) ? 1 : 0));
    json_set(out, "rerun", json_bool(rerun ? 1 : 0));
    json_set(out, "task", task[0] ? json_string(task) : json_null());
    return out;
}

json_t *w20_telegramse_1853c7_mark_recovered(const json_t *adapter) {
    json_t *actions = json_array();
    json_t *out;
    int degraded = ts_flag(adapter, "degraded", 0);
    int rerun = ts_flag(adapter, "rerun", 0);
    char task[256];
    task[0] = '\0';

    if (!degraded) {
        out = ts_result(actions);
        json_set(out, "degraded", json_bool(0));
        json_set(out, "rerun", json_bool(rerun ? 1 : 0));
        json_set(out, "task", json_null());
        return out;
    }
    if (ts_flag(adapter, "running", 0) && !ts_flag(adapter, "fatal", 0)) {
        ts_push(actions, "status:connected");
        ts_schedule(adapter, actions, &rerun, task, sizeof task);
        if (ts_flag(adapter, "has_replay", 0))
            ts_push(actions, "replay");
    }
    out = ts_result(actions);
    json_set(out, "degraded", json_bool(0));
    json_set(out, "rerun", json_bool(rerun ? 1 : 0));
    json_set(out, "task", task[0] ? json_string(task) : json_null());
    return out;
}

json_t *w20_telegramse_1853c7_sweep_loop(const json_t *adapter,
                                        const json_t *delivered,
                                        const json_t *rerun_after) {
    json_t *actions = json_array();
    json_t *out;
    int degraded = ts_flag(adapter, "degraded", 0);
    int running = ts_flag(adapter, "running", 0);
    size_t n_del = (delivered && delivered->type == JSON_ARRAY) ? delivered->c.count : 0;
    size_t n_rer = (rerun_after && rerun_after->type == JSON_ARRAY) ? rerun_after->c.count : 0;
    size_t i = 0;
    int guard;

    for (guard = 0; guard < TS_MAX_ITERS; guard++) {
        int rerun = 0;
        long long count = 0;
        char buf[96];
        ts_push(actions, "sleep");
        if (degraded || !running) break;
        if (i < n_del) count = (long long)delivered->c.items[i]->num_val;
        if (i < n_rer) rerun = ts_truthy(rerun_after->c.items[i]);
        snprintf(buf, sizeof buf, "redeliver:%lld", count);
        ts_push(actions, buf);
        i++;
        if (!rerun) break;
    }
    out = ts_result(actions);
    json_set(out, "calls", json_number((double)i));
    return out;
}
