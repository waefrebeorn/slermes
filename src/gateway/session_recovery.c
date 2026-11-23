/*
 * session_recovery.c — faithful C11 port of gateway/session_recovery.py.
 * Gate w10_sessionrec_51b4df.
 *
 * PoP: w10_sessionrec_51b4df_profile_from_session_key @ gateway/session_recovery.py:_profile_from_session_key
 * PoP: w10_sessionrec_51b4df_recovered_row_allowed @ gateway/session_recovery.py:_recovered_row_allowed_for_active_profile
 * PoP: w10_sessionrec_51b4df_recovered_row_matches_source_scope @ gateway/session_recovery.py:_recovered_row_matches_source_scope
 */
#include "hermes_json.h"
#include "noc_w10_sessionrec_51b4df.h"
#include <stdlib.h>
#include <string.h>

/* session_keys.profile_from_session_key(session_key) */
json_t *w10_sessionrec_51b4df_profile_from_session_key(const char *session_key) {
    if (!session_key || !*session_key) return json_null();
    /* split(':') on a str; need >= 2 parts and parts[0] == 'agent'. */
    const char *first = strchr(session_key, ':');
    if (!first) return json_null();
    if ((size_t)(first - session_key) != 5 || strncmp(session_key, "agent", 5) != 0)
        return json_null();
    const char *slot_start = first + 1;
    const char *second = strchr(slot_start, ':');
    size_t slot_len = second ? (size_t)(second - slot_start) : strlen(slot_start);
    /* profile_from_session_key_namespace: 'main' -> 'default', 'main~' -> 'main'. */
    if (slot_len == 0 || (slot_len == 4 && strncmp(slot_start, "main", 4) == 0))
        return json_string("default");
    if (slot_len == 5 && strncmp(slot_start, "main~", 5) == 0)
        return json_string("main");
    char *buf = (char *)malloc(slot_len + 1);
    if (!buf) return json_null();
    memcpy(buf, slot_start, slot_len);
    buf[slot_len] = '\0';
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *w10_sessionrec_51b4df_recovered_row_allowed(const char *requested_session_key,
                                                    const struct json_t *recovered,
                                                    int multiplex_profiles,
                                                    const char *active_profile) {
    const char *rkey = json_get_str(recovered, "session_key", "");
    if (!rkey) rkey = "";
    if (!*rkey || !requested_session_key || strcmp(rkey, requested_session_key) == 0)
        return json_bool(true);
    json_t *rp = w10_sessionrec_51b4df_profile_from_session_key(rkey);
    const char *rprofile = (rp && rp->type == JSON_STRING) ? rp->str_val : NULL;
    if (!rprofile) { json_free(rp); return json_bool(true); }
    int allowed;
    if (multiplex_profiles) {
        json_t *qp = w10_sessionrec_51b4df_profile_from_session_key(requested_session_key);
        const char *qprofile = (qp && qp->type == JSON_STRING) ? qp->str_val : NULL;
        allowed = (qprofile == NULL) || strcmp(rprofile, qprofile) == 0;
        json_free(qp);
    } else {
        allowed = active_profile && strcmp(rprofile, active_profile) == 0;
    }
    json_free(rp);
    return json_bool(allowed ? true : false);
}

json_t *w10_sessionrec_51b4df_recovered_row_matches_source_scope(const struct json_t *recovered,
                                                                 const struct json_t *source) {
    const char *platform = json_get_str(source, "platform", "");
    const char *chat_type = json_get_str(source, "chat_type", "");
    const char *scope_id = json_get_str(source, "scope_id", "");
    /* if platform != SLACK or chat_type == 'dm' or not scope_id: return True */
    if (strcmp(platform, "slack") != 0 || (chat_type && strcmp(chat_type, "dm") == 0)
        || !scope_id || !*scope_id)
        return json_bool(true);
    json_t *origin = NULL;
    const char *origin_text = json_get_str(recovered, "origin_json", "");
    if (origin_text && *origin_text) origin = json_parse(origin_text, NULL);
    if (!origin || origin->type != JSON_OBJECT) {
        if (origin) json_free(origin);
        return json_bool(false);
    }
    /* origin.get('scope_id', origin.get('guild_id')) == source.scope_id */
    json_t *val = json_obj_get(origin, "scope_id");
    if (!val) val = json_obj_get(origin, "guild_id");
    int matched = val && val->type == JSON_STRING && val->str_val
        && strcmp(val->str_val, scope_id) == 0;
    json_free(origin);
    return json_bool(matched ? true : false);
}
