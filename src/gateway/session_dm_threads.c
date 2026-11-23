/*
 * session_dm_threads.c — C port of upstream Hermes gateway/session_dm_threads.py.
 *
 * PoP: w13_sessiondm_83038a_declared_topic @ gateway/session_dm_threads.py:_declared_topic_thread
 * PoP: w13_sessiondm_83038a_key_source @ gateway/session_dm_threads.py:dm_thread_key_source
 * PoP: w13_sessiondm_83038a_batch_suffix @ gateway/session_dm_threads.py:dm_thread_batch_suffix
 *
 * The source object is a JSON seam; platform/topic-mode collaborators are
 * scripted by the fixture. No session store or adapter is touched.
 */
#include "noc_w13_sessiondm_83038a.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_NULL: return 0;
        default: return json_object_size(v) > 0 || (v->type == JSON_ARRAY && json_len(v) > 0);
    }
}
static void lower_strip(const json_t *v, char *out, size_t cap) {
    out[0] = '\0';
    const char *s = jstr(v);
    if (!s) return;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    size_t k = 0;
    for (size_t i = a; i < b && k + 1 < cap; i++) out[k++] = (char)tolower((unsigned char)s[i]);
    out[k] = '\0';
}

json_t *w13_sessiondm_83038a_declared_topic(const struct json_t *extra,
                                             const struct json_t *chat_id,
                                             const struct json_t *thread_id) {
    if (!json_is_object(extra)) return json_bool(0);
    const char *want_chat = jstr(chat_id);
    const char *want_thread = jstr(thread_id);
    const json_t *topics = json_obj_get(extra, "dm_topics");
    if (!json_is_array(topics)) return json_bool(0);
    for (size_t i = 0; i < json_len(topics); i++) {
        const json_t *entry = json_get(topics, i);
        if (!json_is_object(entry)) continue;
        const char *entry_chat = jstr(json_obj_get(entry, "chat_id"));
        if (!entry_chat || !want_chat || strcmp(entry_chat, want_chat) != 0) continue;
        const json_t *list = json_obj_get(entry, "topics");
        if (!json_is_array(list)) continue;
        for (size_t j = 0; j < json_len(list); j++) {
            const json_t *topic = json_get(list, j);
            if (!json_is_object(topic)) continue;
            const char *tid = jstr(json_obj_get(topic, "thread_id"));
            const char *lhs = tid ? tid : ""; /* str(topic.get("thread_id") or "") */
            const char *rhs = want_thread ? want_thread : "";
            if (!strcmp(lhs, rhs)) return json_bool(1);
        }
    }
    return json_bool(0);
}

/* Whether dm_thread_key_source drops the DM thread from the session key. */
static int thread_dropped(const struct json_t *source, const struct json_t *extra, const struct json_t *db) {
    if (!json_is_object(source)) return 0;
    const char *platform = jstr(json_obj_get(source, "platform"));
    const char *chat_type = jstr(json_obj_get(source, "chat_type"));
    const char *thread = jstr(json_obj_get(source, "thread_id"));
    if (!platform || strcmp(platform, "telegram") != 0) return 0;
    if (!chat_type || strcmp(chat_type, "dm") != 0) return 0;
    if (!thread || !*thread) return 0;
    if (truthy(json_obj_get(source, "detached_route"))) return 0;
    char mode[128];
    lower_strip(json_obj_get(extra, "dm_threads"), mode, sizeof mode);
    if (!mode[0]) strcpy(mode, "per_thread");
    if (strcmp(mode, "shared") != 0) return 0;
    /* declared topic? */
    const char *chat = jstr(json_obj_get(source, "chat_id"));
    if (json_is_object(extra)) {
        const json_t *topics = json_obj_get(extra, "dm_topics");
        if (json_is_array(topics)) {
            for (size_t i = 0; i < json_len(topics); i++) {
                const json_t *entry = json_get(topics, i);
                if (!json_is_object(entry)) continue;
                const char *ec = jstr(json_obj_get(entry, "chat_id"));
                if (!ec || !chat || strcmp(ec, chat) != 0) continue;
                const json_t *list = json_obj_get(entry, "topics");
                if (!json_is_array(list)) continue;
                for (size_t j = 0; j < json_len(list); j++) {
                    const json_t *topic = json_get(list, j);
                    const char *tid = json_is_object(topic) ? jstr(json_obj_get(topic, "thread_id")) : NULL;
                    if (tid && !strcmp(tid, thread)) return 0;
                }
            }
        }
    }
    /* topic-mode enabled keeps the thread in the key */
    if (json_is_object(db) && json_obj_get(db, "topic_mode") && truthy(json_obj_get(db, "topic_mode"))) return 0;
    return 1;
}

json_t *w13_sessiondm_83038a_key_source(const struct json_t *source,
                                         const struct json_t *extra,
                                         const struct json_t *db) {
    if (!thread_dropped(source, extra, db)) {
        const char *thread = json_is_object(source) ? jstr(json_obj_get(source, "thread_id")) : NULL;
        return thread ? json_string(thread) : json_null();
    }
    return json_null(); /* replace_source(source, thread_id=None) */
}

json_t *w13_sessiondm_83038a_batch_suffix(const struct json_t *source,
                                           const struct json_t *extra) {
    if (!thread_dropped(source, extra, NULL)) return json_string("");
    const char *thread = json_is_object(source) ? jstr(json_obj_get(source, "thread_id")) : NULL;
    size_t n = thread ? strlen(thread) : 0;
    char *buf = (char *)malloc(n + sizeof(":thread:"));
    if (!buf) return json_string("");
    memcpy(buf, ":thread:", 8);
    if (n) memcpy(buf + 8, thread, n);
    buf[8 + n] = '\0';
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

