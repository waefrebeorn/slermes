/*
 * slash_commands_login.c — C port of upstream Hermes gateway/slash_commands_login.py.
 *
 * PoP: w16_slashcomma_6b9c63_paired_dm @ gateway/slash_commands_login.py:_handle_login_command
 * PoP: w16_slashcomma_6b9c63_attempt_key @ gateway/slash_commands_login.py:_handle_login_command
 *
 * Both mirror the live prefix of _handle_login_command:
 *   platform = str(getattr(src.platform, "value", src.platform)).lower()
 *   paired_dm = chat_type in {"dm","private"} and bool(chat_id) and platform not in blocked
 *   key = (str(...platform...), str(src.chat_id), str(getattr(src,"user_id","") or ""))
 * The blocked set is read from the live _LOGIN_BLOCKED_PLATFORMS by the oracle.
 */
#include "noc_w16_slashcomma_6b9c63.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* getattr(src.platform, "value", src.platform), then str() in the fixture domain. */
static char *platform_str(const json_t *platform) {
    const json_t *v = platform;
    if (platform && platform->type == JSON_OBJECT && json_obj_get(platform, "value"))
        v = json_obj_get(platform, "value");
    if (!v || v->type == JSON_NULL) return dupstr("None");   /* str(None) */
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupstr(buf);
    }
    return dupstr("{}");
}

static int py_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return !v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return !v->str_val || !v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count == 0;
    return 1;
}

static char *py_str_or_empty(const json_t *v) {
    if (py_falsy(v)) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupstr(buf);
    }
    return dupstr("[]");
}

int w16_slashcomma_6b9c63_paired_dm(const json_t *platform, const char *chat_type, const json_t *chat_id) {
    char *p = platform_str(platform);
    for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);
    int blocked = (!strcmp(p, "ntfy") || !strcmp(p, "raft") || !strcmp(p, "a2a"));
    int ct_ok = (chat_type && (!strcmp(chat_type, "dm") || !strcmp(chat_type, "private")));
    int cid_ok = !py_falsy(chat_id);
    int result = ct_ok && cid_ok && !blocked;
    free(p);
    return result ? 1 : 0;
}

json_t *w16_slashcomma_6b9c63_attempt_key(const json_t *platform, const json_t *chat_id, const json_t *user_id) {
    json_t *arr = json_array();
    char *p = platform_str(platform);
    json_append(arr, json_string(p));
    free(p);
    char *cid = py_str_or_empty(chat_id);
    if (chat_id && chat_id->type == JSON_NULL) { free(cid); cid = dupstr("None"); }
    json_append(arr, json_string(cid));
    free(cid);
    char *uid = py_str_or_empty(user_id);
    json_append(arr, json_string(uid));
    free(uid);
    return arr;
}

