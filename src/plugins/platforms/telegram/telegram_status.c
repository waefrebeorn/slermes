/*
 * telegram_status.c -- real C11 port of the serialized editable-status
 * bookkeeping of upstream Hermes plugins/platforms/telegram/telegram_status.py.
 *
 * PoP: w21_telegramst_ddf41d_send_or_update @ plugins/platforms/telegram/telegram_status.py:send_or_update_status
 * PoP: w21_telegramst_ddf41d_py_str @ plugins/platforms/telegram/telegram_status.py:send_or_update_status
 *
 * The adapter is the seam: `edit_message`/`send` answers, the id map and what
 * happens to it while the edit is awaited all arrive from the caller, so the
 * whole coroutine body runs here with no await left.
 */
#include "noc_w21_telegramst_ddf41d.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ts_dup(const char *s)
{
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ts_truthy(const json_t *v)
{
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

char *w21_telegramst_ddf41d_py_str(const json_t *value)
{
    char buf[64];

    if (!value) return ts_dup("None");
    switch (value->type) {
    case JSON_STRING: return ts_dup(value->str_val ? value->str_val : "");
    case JSON_BOOL:   return ts_dup(value->bool_val ? "True" : "False");
    case JSON_NULL:   return ts_dup("None");
    case JSON_NUMBER:
        if (isfinite(value->num_val) && floor(value->num_val) == value->num_val &&
            fabs(value->num_val) < 9007199254740992.0)
            snprintf(buf, sizeof(buf), "%.0f", value->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", value->num_val);
        return ts_dup(buf);
    case JSON_ARRAY:
    case JSON_OBJECT: {
        char *dump = json_dumps(value, 0);
        char *out = ts_dup(dump ? dump : "");
        free(dump);
        return out;
    }
    }
    return ts_dup("");
}

/* --- the id map: [["<chat>","<status key>","<message id>"], ...] ------------- */

static const char *triple_field(const json_t *arr, size_t index, size_t field)
{
    const json_t *t;
    const json_t *v;

    if (!arr || arr->type != JSON_ARRAY || index >= arr->c.count || !arr->c.items) return NULL;
    t = arr->c.items[index];
    if (!t || t->type != JSON_ARRAY || field >= t->c.count || !t->c.items) return NULL;
    v = t->c.items[field];
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static long triple_find(const json_t *arr, const char *chat, const char *key)
{
    size_t i;

    if (!arr || arr->type != JSON_ARRAY) return -1;
    for (i = 0; i < arr->c.count; i++) {
        const char *c = triple_field(arr, i, 0);
        const char *k = triple_field(arr, i, 1);
        if (c && k && !strcmp(c, chat) && !strcmp(k, key)) return (long)i;
    }
    return -1;
}

static void triple_set_id(json_t *arr, size_t index, const char *id)
{
    json_t *t;
    if (!arr || arr->type != JSON_ARRAY || index >= arr->c.count) return;
    t = arr->c.items[index];
    if (!t || t->type != JSON_ARRAY || t->c.count < 3) return;
    json_free(t->c.items[2]);
    t->c.items[2] = json_string(id ? id : "");
}

static void triple_remove(json_t *arr, size_t index)
{
    if (!arr || arr->type != JSON_ARRAY || index >= arr->c.count) return;
    json_free(arr->c.items[index]);
    json_array_remove(arr, index);
}

static void triple_append(json_t *arr, const char *chat, const char *key, const char *id)
{
    json_t *t = json_array();
    if (!t) return;
    json_append(t, json_string(chat));
    json_append(t, json_string(key));
    json_append(t, json_string(id ? id : ""));
    json_append(arr, t);
}

/* The SendResult the adapter answered with, as the record carries it. */
static json_t *answer_record(const json_t *spec)
{
    json_t *out = json_object();
    const json_t *ok = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "success") : NULL;
    const json_t *mid = (spec && spec->type == JSON_OBJECT) ? json_obj_get(spec, "message_id") : NULL;

    if (!out) return NULL;
    json_set(out, "success", json_bool(ts_truthy(ok)));
    json_set(out, "message_id", mid ? json_copy(mid) : json_null());
    return out;
}

json_t *w21_telegramst_ddf41d_send_or_update(const json_t *chat_id, const json_t *status_key,
                                             const char *content, const w21_telegramst_ddf41d_env *e)
{
    char *chat = w21_telegramst_ddf41d_py_str(chat_id);
    char *key = w21_telegramst_ddf41d_py_str(status_key);
    json_t *ids, *ops, *out, *result = NULL;
    long long max_ids = (e && e->max_ids >= 0) ? e->max_ids : W21_TELEGRAMST_DDF41D_STATUS_IDS_MAX;
    long idx;
    int lock_created = (e && e->lock_exists) ? 0 : 1;
    char *cached = NULL;

    (void)content;
    if (!chat || !key) { free(chat); free(key); return NULL; }

    ids = (e && e->message_ids && e->message_ids->type == JSON_ARRAY)
              ? json_copy(e->message_ids)
              : json_array();
    ops = json_array();
    if (!ids || !ops) { json_free(ids); json_free(ops); free(chat); free(key); return NULL; }

    idx = triple_find(ids, chat, key);
    if (idx >= 0) cached = ts_dup(triple_field(ids, (size_t)idx, 2));

    if (cached) {
        json_t *edit;
        json_append(ops, json_string("edit_message"));
        /* the awaited edit_message: another caller may evict or replace the key */
        if (e && e->during_edit) {
            if (!strcmp(e->during_edit, "replace")) {
                triple_set_id(ids, (size_t)idx, "other");
            } else if (!strcmp(e->during_edit, "pop")) {
                triple_remove(ids, (size_t)idx);
            }
        }
        edit = answer_record(e ? e->edit : NULL);
        if (!edit) { json_free(ids); json_free(ops); free(cached); free(chat); free(key); return NULL; }
        if (ts_truthy(json_obj_get(edit, "success"))) {
            const json_t *mid = json_obj_get(edit, "message_id");
            long now = triple_find(ids, chat, key);
            const char *current = now >= 0 ? triple_field(ids, (size_t)now, 2) : NULL;
            if (ts_truthy(mid) && current && !strcmp(current, cached)) {
                char *new_id = w21_telegramst_ddf41d_py_str(mid);
                triple_set_id(ids, (size_t)now, new_id ? new_id : "");
                free(new_id);
            }
            result = edit;
        } else {
            long now = triple_find(ids, chat, key);
            json_free(edit);
            if (now >= 0) triple_remove(ids, (size_t)now);
        }
    }

    if (!result) {
        json_t *sent;
        json_append(ops, json_string("send"));
        sent = answer_record(e ? e->send : NULL);
        if (!sent) { json_free(ids); json_free(ops); free(cached); free(chat); free(key); return NULL; }
        if (ts_truthy(json_obj_get(sent, "success")) && ts_truthy(json_obj_get(sent, "message_id"))) {
            size_t count = ids->c.count;
            size_t drop, i;
            long now;
            char *new_id;
            if ((long long)count >= max_ids) {
                drop = (size_t)(max_ids / 2);
                for (i = 0; i < drop && ids->c.count > 0; i++) triple_remove(ids, 0);
            }
            new_id = w21_telegramst_ddf41d_py_str(json_obj_get(sent, "message_id"));
            now = triple_find(ids, chat, key);
            if (now >= 0) triple_set_id(ids, (size_t)now, new_id ? new_id : "");
            else triple_append(ids, chat, key, new_id ? new_id : "");
            free(new_id);
        }
        result = sent;
    }

    out = json_object();
    if (!out) { json_free(ids); json_free(ops); json_free(result); free(cached); free(chat); free(key); return NULL; }
    json_set(out, "result", result);
    json_set(out, "ids", ids);
    json_set(out, "lock_created", json_bool(lock_created));
    json_set(out, "ops", ops);
    free(cached);
    free(chat);
    free(key);
    return out;
}
