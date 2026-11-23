/*
 * browser_session.c — faithful C11 port of upstream Hermes tools/browser_session.py.
 * PoP: w8_browsersess_acacf0_is_local_sidecar_key @ tools/browser_session.py:is_local_sidecar_key
 * PoP: w8_browsersess_acacf0_owner_task_id @ tools/browser_session.py:owner_task_id
 * PoP: w8_browsersess_acacf0_session_record_ex @ tools/browser_session.py:_session_record
 * PoP: w8_browsersess_acacf0_publish @ tools/browser_session.py:_publish
 * PoP: w8_browsersess_acacf0_shares_bot_desktop_ex @ tools/browser_session.py:shares_bot_desktop_browser
 * PoP: w8_browsersess_acacf0_human_holds_shared_ex @ tools/browser_session.py:human_holds_shared_browser
 * PoP: w8_browsersess_acacf0_constants @ tools/browser_session.py:LOCAL_SUFFIX/BROWSER_ORPHAN_REAP_INTERVAL
 */
#include "hermes_json.h"
#include "noc_w8_browsersess_acacf0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const LOCAL_SUFFIX = "::local";

int w8_browsersess_acacf0_is_local_sidecar_key(const char *session_key) {
    if (!session_key) return 0;
    size_t n = strlen(session_key), s = strlen(LOCAL_SUFFIX);
    return n >= s && !strcmp(session_key + n - s, LOCAL_SUFFIX);
}
char *w8_browsersess_acacf0_owner_task_id(const char *session_key) {
    const char *key = session_key ? session_key : "";
    size_t n = strlen(key), s = strlen(LOCAL_SUFFIX);
    size_t effective = (n >= s && !strcmp(key + n - s, LOCAL_SUFFIX)) ? n - s : n;
    char *tmp = (char *)malloc(effective + 1);
    if (!tmp) return NULL;
    memcpy(tmp, key, effective); tmp[effective] = 0;
    char *at = strrchr(tmp, '@');
    if (at) *at = 0;
    return tmp;
}
struct json_t *w8_browsersess_acacf0_session_record_ex(const char *prefix, const struct json_t *cdp_url,
                                             const struct json_t *features, const char *hex10) {
    struct json_t *o = json_object();
    char name[512];
    snprintf(name, sizeof(name), "%s_%s", prefix ? prefix : "", hex10 ? hex10 : "");
    json_set(o, "session_name", json_string(name));
    json_set(o, "bb_session_id", json_null());
    json_set(o, "cdp_url", cdp_url ? json_copy(cdp_url) : json_null());
    json_set(o, "features", features ? json_copy(features) : json_null());
    return o;
}
static void setdefault(struct json_t *o, const char *k, struct json_t *v) {
    if (!json_obj_get(o, k)) json_set(o, k, v); else json_free(v);
}
struct json_t *w8_browsersess_acacf0_publish(const char *cache_key, const struct json_t *session_info,
                                   const char *home_key) {
    struct json_t *o = session_info ? json_copy(session_info) : json_object();
    char *owner = w8_browsersess_acacf0_owner_task_id(cache_key);
    setdefault(o, "session_key", json_string(cache_key ? cache_key : ""));
    setdefault(o, "owner_task_id", json_string(owner ? owner : ""));
    setdefault(o, "home_key", json_string(home_key ? home_key : ""));
    free(owner);
    return o;
}
static int local_feature(const struct json_t *session_info) {
    if (!session_info || session_info->type != JSON_OBJECT) return 0;
    const struct json_t *f = json_obj_get(session_info, "features");
    if (!f || f->type != JSON_OBJECT) return 0;
    const struct json_t *v = json_obj_get(f, "local");
    if (!v || v->type != JSON_BOOL) return 0;
    return v->bool_val ? 1 : 0;
}
int w8_browsersess_acacf0_shares_bot_desktop_ex(const struct json_t *session_info, const char *display,
                                      int human_holds) {
    if (!local_feature(session_info)) return 0;
    return (display && *display) ? 1 : (human_holds ? 1 : 0);
}
int w8_browsersess_acacf0_human_holds_shared_ex(const struct json_t *session_info, const char *display,
                                      int human_holds) {
    if (!w8_browsersess_acacf0_shares_bot_desktop_ex(session_info, display, human_holds)) return 0;
    return human_holds ? 1 : 0;
}
struct json_t *w8_browsersess_acacf0_constants(void) {
    struct json_t *o = json_object();
    json_set(o, "local_suffix", json_string(LOCAL_SUFFIX));
    json_set(o, "session_inactivity_timeout", json_number(120));
    json_set(o, "w8_browsersess_acacf0_inactivity_timeout", json_number(120));
    json_set(o, "orphan_reap_interval", json_number(300));
    json_set(o, "max_inactivity_cleanup_failures", json_number(3));
    json_set(o, "stale_endpoint_retries", json_number(2));
    return o;
}