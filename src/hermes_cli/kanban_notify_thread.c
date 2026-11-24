/*
 * kanban_notify_thread.c - C11 port of upstream hermes_cli/kanban_notify_thread.py
 * (gate w25_kanbannoti_2979d2).
 *
 * PoP: w25_kanbannoti_2979d2_chat_threads @ hermes_cli/kanban_notify_thread.py:_chat_threads
 * PoP: w25_kanbannoti_2979d2_unsubscribe_thread @ hermes_cli/kanban_notify_thread.py:resolve_unsubscribe_thread
 * PoP: w25_kanbannoti_2979d2_subscribe_thread @ hermes_cli/kanban_notify_thread.py:resolve_subscribe_thread
 *
 * The notify-subscription read (kanban_db_notify.list_notify_subs) and the
 * caller's session env (gateway.session_context.get_session_env) are seams:
 * the rows and the session identity are fixtures here.  What is ported is the
 * resolution order and the stderr report, byte for byte.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_kanbannoti_2979d2.h"

#define KB_TELEGRAM_WARNING \
    "warning: no --thread-id given and none to infer; the completion ping will " \
    "go to the chat's DM root, not a topic"

static char *kb_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *kb_lower(const char *s) {
    size_t n, i;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

/* Python str() of a JSON value. */
static char *kb_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return kb_dup("None");
    switch (v->type) {
    case JSON_NULL:
        break;
    case JSON_BOOL:
        return kb_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val < 1e15 && v->num_val > -1e15)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return kb_dup(buf);
    case JSON_STRING:
        return kb_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:
        return kb_dup("[]");
    case JSON_OBJECT:
        return kb_dup("{}");
    }
    return kb_dup("None");
}

/* d.get(key, absent): the absent default only when the key is missing. */
static char *kb_field(const json_t *obj, const char *key, const char *absent) {
    const json_t *v = obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
    if (!v) return kb_dup(absent);
    return kb_str(v);
}

static int kb_truthy(const json_t *v) {
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

/* _chat_threads(subs, args): thread ids (None = root lane) for the target chat. */
static json_t *kb_chat_threads(const json_t *subs, const char *platform,
                               const json_t *chat_id_raw) {
    json_t *out = json_array();
    char *want_platform = kb_lower(platform);
    char *want_chat = kb_str(chat_id_raw);
    size_t i, n;

    if (!want_platform || !want_chat) {
        free(want_platform); free(want_chat);
        return out;
    }
    if (!subs || subs->type != JSON_ARRAY) {
        free(want_platform); free(want_chat);
        return out;
    }
    n = json_len(subs);
    for (i = 0; i < n; i++) {
        const json_t *sub = json_get(subs, i);
        char *sp, *sp_l, *sc, *tid;
        const json_t *thread_id;
        if (!sub || sub->type != JSON_OBJECT) continue;
        sp = kb_field(sub, "platform", "");
        sc = kb_field(sub, "chat_id", "");
        sp_l = kb_lower(sp);
        if (sp_l && sc && strcmp(sp_l, want_platform) == 0 && strcmp(sc, want_chat) == 0) {
            thread_id = json_obj_get(sub, "thread_id");
            if (kb_truthy(thread_id)) {
                tid = kb_str(thread_id);
                json_append(out, tid ? json_string(tid) : json_null());
                free(tid);
            } else {
                json_append(out, json_null());
            }
        }
        free(sp); free(sc); free(sp_l);
    }
    free(want_platform);
    free(want_chat);
    return out;
}

/* _session_thread_for(platform, chat_id) with the session env pinned. */
static char *kb_session_thread(const json_t *req, const char *platform,
                               const json_t *chat_id_raw) {
    const json_t *s = json_obj_get(req, "session");
    char *sp, *sp_l, *pl, *sc, *want_chat, *th = NULL;

    if (!s || s->type != JSON_OBJECT) return NULL;
    sp = kb_field(s, "platform", "");
    sc = kb_field(s, "chat_id", "");
    sp_l = kb_lower(sp);
    pl = kb_lower(platform);
    want_chat = kb_str(chat_id_raw);
    if (sp_l && pl && sc && want_chat &&
        strcmp(sp_l, pl) == 0 && strcmp(sc, want_chat) == 0) {
        char *raw = kb_field(s, "thread_id", "");
        if (raw && raw[0] != '\0') th = raw;
        else free(raw);
    }
    free(sp); free(sc); free(sp_l); free(pl); free(want_chat);
    return th;
}

static int kb_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w25_kanbannoti_2979d2_chat_threads(const json_t *req) {
    return kb_chat_threads(json_obj_get(req, "subs"), json_get_str(req, "platform", ""),
                           json_obj_get(req, "chat_id"));
}

json_t *w25_kanbannoti_2979d2_unsubscribe_thread(const json_t *req) {
    const json_t *explicit_thread = json_obj_get(req, "thread_id");
    json_t *threads;
    char *session_thread;
    size_t i, n;

    if (explicit_thread && explicit_thread->type != JSON_NULL)
        return json_copy(explicit_thread);
    threads = kb_chat_threads(json_obj_get(req, "subs"), json_get_str(req, "platform", ""),
                              json_obj_get(req, "chat_id"));
    n = json_len(threads);
    for (i = 0; i < n; i++) {
        const json_t *t = json_get(threads, i);
        if (!t || t->type == JSON_NULL) { json_free(threads); return json_null(); }
    }
    session_thread = kb_session_thread(req, json_get_str(req, "platform", ""),
                                       json_obj_get(req, "chat_id"));
    if (session_thread) {
        for (i = 0; i < n; i++) {
            const json_t *t = json_get(threads, i);
            if (t && t->type == JSON_STRING && t->str_val &&
                strcmp(t->str_val, session_thread) == 0) {
                json_t *out = json_string(session_thread);
                free(session_thread);
                json_free(threads);
                return out;
            }
        }
        free(session_thread);
    }
    if (n == 1) {
        json_t *out = json_copy(json_get(threads, 0));
        json_free(threads);
        return out;
    }
    json_free(threads);
    return json_null();
}

json_t *w25_kanbannoti_2979d2_subscribe_thread(const json_t *req) {
    const json_t *explicit_thread = json_obj_get(req, "thread_id");
    const json_t *chat_type = json_obj_get(req, "chat_type");
    const char *platform = json_get_str(req, "platform", "");
    char *chat_id_str;
    char *session_thread, *notice = NULL, *chosen = NULL;
    json_t *out;

    out = json_object();
    chat_id_str = kb_str(json_obj_get(req, "chat_id"));
    if (explicit_thread && explicit_thread->type != JSON_NULL) {
        json_set(out, "thread_id", json_copy(explicit_thread));
        json_set(out, "notice", json_null());
        free(chat_id_str);
        return out;
    }
    session_thread = kb_session_thread(req, platform, json_obj_get(req, "chat_id"));
    if (session_thread) {
        size_t need = strlen(session_thread) + 96;
        notice = (char *)malloc(need);
        if (notice)
            snprintf(notice, need,
                     "(no --thread-id given; using this session's thread_id=%s)",
                     session_thread);
        chosen = kb_dup(session_thread);
        free(session_thread);
    } else {
        json_t *threads = kb_chat_threads(json_obj_get(req, "subs"), platform,
                                          json_obj_get(req, "chat_id"));
        size_t n = json_len(threads), i, count = 0;
        const char **names = NULL;

        if (n) names = (const char **)calloc(n, sizeof *names);
        for (i = 0; i < n && names; i++) {
            const json_t *t = json_get(threads, i);
            if (t && t->type == JSON_STRING && t->str_val && t->str_val[0] != '\0')
                names[count++] = t->str_val;
        }
        if (count > 0 && names) {
            size_t need, j;
            qsort((void *)names, count, sizeof *names, kb_cmp_str);
            need = strlen(names[0]) + strlen(platform)
                   + (chat_id_str ? strlen(chat_id_str) : 0) + 160;
            for (j = 1; j < count; j++) need += strlen(names[j]) + 3;
            notice = (char *)malloc(need);
            if (notice) {
                int used = snprintf(notice, need,
                                    "(no --thread-id given; inherited thread_id=%s from this "
                                    "task's existing %s:%s subscription",
                                    names[0], platform, chat_id_str ? chat_id_str : "");
                if (count > 1 && used > 0 && (size_t)used < need) {
                    used += snprintf(notice + used, need - (size_t)used, " (others: ");
                    for (j = 1; j < count && used > 0 && (size_t)used < need; j++)
                        used += snprintf(notice + used, need - (size_t)used, "%s%s",
                                         j > 1 ? ", " : "", names[j]);
                    if (used > 0 && (size_t)used < need)
                        used += snprintf(notice + used, need - (size_t)used,
                                         "; pass --thread-id to choose)");
                }
                if (used > 0 && (size_t)used < need)
                    snprintf(notice + used, need - (size_t)used, ")");
                chosen = kb_dup(names[0]);
            }
        } else if (strcmp(platform, "telegram") == 0 &&
                   (!chat_type || chat_type->type == JSON_NULL ||
                    (chat_type->type == JSON_STRING && chat_type->str_val &&
                     strcmp(chat_type->str_val, "dm") == 0))) {
            notice = kb_dup(KB_TELEGRAM_WARNING);
        }
        free(names);
        json_free(threads);
    }
    if (chosen) json_set(out, "thread_id", json_string(chosen));
    else json_set(out, "thread_id", json_null());
    if (notice && notice[0] != '\0') json_set(out, "notice", json_string(notice));
    else json_set(out, "notice", json_null());
    free(notice);
    free(chosen);
    free(chat_id_str);
    return out;
}
