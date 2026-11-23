/*
 * bot_chat_legacy.c — real C11 port of upstream Hermes cron/bot_chat_legacy.py.
 *
 * PoP: botchat_legacy_malformed @ cron/bot_chat_legacy.py:_malformed
 * PoP: botchat_legacy_defer_record @ cron/bot_chat_legacy.py:defer
 *
 * The drain/dispose paths are orchestration around filesystem and gateway
 * delivery (injected by the oracle). The two deterministic helpers above are
 * ported verbatim, including Python's `dict.get` truthiness, the ordered
 * "missing" list and the id/name-only job projection in defer().
 */
#include "noc_w7_botchatleg_2b179a.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *botchat_legacy_malformed(const json_t *record) {
    static const char *const keys[] = {"id", "home", "content"};
    char msg[512], keys_buf[64];
    size_t pos, i;
    int any = 0;

    if (!record || record->type != JSON_OBJECT)
        return json_string("malformed legacy bot-chat record (missing id, home, content); not delivered");

    pos = 0;
    for (i = 0; i < 3; i++) {
        json_t *v = json_obj_get(record, keys[i]);
        int usable = v && v->type == JSON_STRING && v->str_val[0];
        if (!usable) {
            size_t klen;
            if (any && pos + 2 < sizeof(keys_buf)) {
                keys_buf[pos++] = ',';
                keys_buf[pos++] = ' ';
            }
            klen = strlen(keys[i]);
            if (pos + klen < sizeof(keys_buf)) {
                memcpy(keys_buf + pos, keys[i], klen);
                pos += klen;
            }
            any = 1;
        }
    }
    keys_buf[pos] = '\0';
    if (any) {
        snprintf(msg, sizeof(msg),
                 "malformed legacy bot-chat record (missing %s); not delivered", keys_buf);
        return json_string(msg);
    }

    /* `record.get("job") or {}` then isinstance(dict) check */
    {
        json_t *job = json_obj_get(record, "job");
        int falsy = !job || job->type == JSON_NULL
                 || (job->type == JSON_BOOL && !job->bool_val)
                 || (job->type == JSON_NUMBER && job->num_val == 0.0)
                 || (job->type == JSON_STRING && !job->str_val[0])
                 || (job->type == JSON_ARRAY && json_len(job) == 0)
                 || (job->type == JSON_OBJECT && json_object_size(job) == 0);
        if (!falsy && job->type != JSON_OBJECT)
            return json_string("malformed legacy bot-chat record (job is not an object); not delivered");
    }
    return json_string("");
}

json_t *botchat_legacy_defer_record(const char *key, const json_t *job,
                                    const char *content, const char *home,
                                    int for_failure, const char *sequence) {
    json_t *out = json_object();
    json_t *small_job = json_object();
    long long seq = strtoll(sequence ? sequence : "0", NULL, 10);

    if (job && job->type == JSON_OBJECT) {
        json_t *v = json_obj_get(job, "id");
        if (v) json_set(small_job, "id", json_copy(v));
        v = json_obj_get(job, "name");
        if (v) json_set(small_job, "name", json_copy(v));
    }
    json_set(out, "id", json_string(key ? key : ""));
    json_set(out, "status", json_string("queued"));
    json_set(out, "job", small_job);
    json_set(out, "content", json_string(content ? content : ""));
    json_set(out, "home", json_string(home ? home : ""));
    json_set(out, "for_failure", json_bool(for_failure ? 1 : 0));
    json_set(out, "sequence", json_number((double)seq));
    return out;
}

