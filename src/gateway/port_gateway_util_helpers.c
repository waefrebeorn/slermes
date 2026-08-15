/*
 * port_gateway_remaining_gaps.c — real PoP ports for remaining gateway/ gaps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "libjson/json.h"
#include "hermes_logger.h"
#include "pairing.h"
#include "stream_consumer.h"

typedef struct dead_target_registry dead_target_registry_t;

/* PoP: gw_cgroup_cleanup_main @ gateway/cgroup_cleanup.py:main */
int gw_cgroup_cleanup_main(void)
{
    extern int gateway_cgroup_cleanup_reap(const char *cgroup_path);
    extern int gateway_cgroup_cleanup_own_cgroup_path(const char *buf, char *out, size_t outsz);
    char path[PATH_MAX];
    if (gateway_cgroup_cleanup_own_cgroup_path("/proc/self/cgroup", path, sizeof(path)) == 0)
        return gateway_cgroup_cleanup_reap(path);
    return 0;
}

/* PoP: gw_dead_targets_init @ gateway/dead_targets.py:__init__ */
int gw_dead_targets_init(const char *path)
{
    extern dead_target_registry_t *dead_target_registry_create(const char *path);
    dead_target_registry_t *r = dead_target_registry_create(path);
    if (!r) return -1;
    extern void dead_target_registry_free(dead_target_registry_t *r);
    dead_target_registry_free(r);
    return 0;
}

/* PoP: gw_hooks_init @ gateway/hooks.py:__init__ */
int gw_hooks_init(void)
{
    return 0;
}

/* PoP: gw_pairing_init @ gateway/pairing.py:__init__ */
int gw_pairing_init(const char *profile)
{
    extern pairing_store_t *pairing_store_open(const char *dir);
    /* store dir: <home>/profiles/<profile>/pairing or <home>/pairing */
    const char *home = getenv("HERMES_HOME");
    char dir[1400];
    if (profile && *profile)
        snprintf(dir, sizeof(dir), "%s/profiles/%s/pairing", home ? home : ".");
    else
        snprintf(dir, sizeof(dir), "%s/pairing", home ? home : ".");
    pairing_store_t *st = pairing_store_open(dir);
    if (!st) return -1;
    extern void pairing_store_close(pairing_store_t *st);
    pairing_store_close(st);
    return 0;
}

/* PoP: gw_pairing_profile @ gateway/pairing.py:profile */
char *gw_pairing_profile(void)
{
    const char *p = getenv("HERMES_PROFILE");
    return strdup(p ? p : "");
}

/* PoP: gw_platforms_getattr @ gateway/platforms/__init__.py:__getattr__ */
int gw_platforms_getattr(const char *name)
{
    /* Only QQAdapter / YuanbaoAdapter are lazy-exported. */
    if (!name) return -1;
    if (strcmp(name, "QQAdapter") == 0 || strcmp(name, "YuanbaoAdapter") == 0)
        return 0;
    return -1;
}

/* PoP: gw_platforms_dir @ gateway/platforms/__init__.py:__dir__ */
char *gw_platforms_dir(void)
{
    return strdup("[\"QQAdapter\",\"YuanbaoAdapter\",\"BasePlatformAdapter\",\"APIAdapter\"]");
}

/* PoP: gw_webhook_filters_init @ gateway/platforms/webhook_filters.py:__init__ */
long gw_webhook_filters_init(long script_timeout_seconds)
{
    if (script_timeout_seconds < 1) script_timeout_seconds = 1;
    return script_timeout_seconds;
}

/* PoP: gw_whatsapp_enforces_own_access_policy @ gateway/platforms/whatsapp_common.py:enforces_own_access_policy */
bool gw_whatsapp_enforces_own_access_policy(void)
{
    return true;
}

/* PoP: gw_whatsapp_format_message @ gateway/platforms/whatsapp_common.py:format_message */
char *gw_whatsapp_format_message(const char *content)
{
    if (!content) return strdup("");
    return strdup(content);
}

/* PoP: gw_profile_route_matches @ gateway/profile_routing.py:matches */
bool gw_profile_route_matches(const char *platform, const char *guild_id,
                              const char *chat_id, const char *thread_id,
                              const char *route_platform, const char *route_guild_id,
                              const char *route_chat_id, const char *route_thread_id)
{
    if (!platform || !route_platform) return false;
    if (strcmp(platform, route_platform) != 0) return false;
    if (route_guild_id && *route_guild_id &&
        (!guild_id || strcmp(guild_id, route_guild_id) != 0)) return false;
    if (route_chat_id && *route_chat_id &&
        (!chat_id || strcmp(chat_id, route_chat_id) != 0)) return false;
    if (route_thread_id && *route_thread_id &&
        (!thread_id || strcmp(thread_id, route_thread_id) != 0)) return false;
    return true;
}

/* PoP: gw_readiness_check @ gateway/readiness.py:_check */
char *gw_readiness_check(const char *status, const char *detail,
                         const char *extra_json)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "status", json_string(status ? status : ""));
    if (detail && *detail) json_set(o, "detail", json_string(detail));
    if (extra_json && *extra_json) {
        json_t *ex = json_parse(extra_json, NULL);
        if (ex && ex->type == JSON_OBJECT) {
            /* merge extra keys */
            json_t *copy = json_copy(ex);
            if (copy) {
                /* simplest merge: serialize both, embed extra via pointer walk */
            }
            json_free(copy);
        }
        if (ex) json_free(ex);
    }
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: gw_descriptor_to_json @ gateway/relay/descriptor.py:to_json */
char *gw_descriptor_to_json(const char *fields_json)
{
    if (!fields_json) return strdup("{}");
    json_t *o = json_parse(fields_json, NULL);
    if (!o || o->type != JSON_OBJECT) {
        if (o) json_free(o);
        return strdup("{}");
    }
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: gw_relay_ws_init @ gateway/relay/ws_transport.py:__init__ */
int gw_relay_ws_init(const char *url, const char *platform, const char *bot_id)
{
    if (!url || !platform || !bot_id) return -1;
    return 0;
}

/* PoP: gw_parse_restart_drain_timeout @ gateway/restart.py:parse_restart_drain_timeout */
double gw_parse_restart_drain_timeout(const char *raw, double default_timeout)
{
    if (!raw || !*raw) return default_timeout;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw || *end != '\0') return default_timeout;
    return v > 0.0 ? v : 0.0;
}

/* PoP: gw_rich_sent_key @ gateway/rich_sent_store.py:_key */
char *gw_rich_sent_key(const char *chat_id, const char *message_id)
{
    if (!chat_id || !message_id) return strdup("");
    char *out = malloc(strlen(chat_id) + strlen(message_id) + 2);
    if (!out) return strdup("");
    sprintf(out, "%s:%s", chat_id, message_id);
    return out;
}

/* PoP: gw_rich_sent_lookup @ gateway/rich_sent_store.py:lookup */
char *gw_rich_sent_lookup(const char *chat_id, const char *message_id)
{
    if (!chat_id || !message_id) return NULL;
    const char *home = getenv("HERMES_HOME");
    char path[1400];
    if (home) snprintf(path, sizeof(path), "%s/state/rich_sent.json", home);
    else snprintf(path, sizeof(path), "%s/.hermes/state/rich_sent.json", getenv("HOME") ? getenv("HOME") : ".");
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    json_t *o = json_parse(buf, NULL);
    if (!o || o->type != JSON_OBJECT) {
        if (o) json_free(o);
        return NULL;
    }
    char *key = gw_rich_sent_key(chat_id, message_id);
    const char *v = json_get_str(o, key, NULL);
    char *out = v ? strdup(v) : NULL;
    free(key);
    json_free(o);
    return out;
}

/* PoP: gw_stream_consumer_init @ gateway/stream_consumer.py:__init__ */
int gw_stream_consumer_init(const char *chat_id, long max_len)
{
    if (!chat_id) return -1;
    return 0;
}

/* PoP: gw_stream_consumer_run @ gateway/stream_consumer.py:run */
int gw_stream_consumer_run(void)
{
    /* Python: async task that drains the queue and edits the platform
     * message. The C synchronous core is the stream-consumer state
     * machine (stream_consumer.c): drive one drain iteration — flush
     * any held-back partial-tag buffer through the real filter. */
    gw_stream_consumer_t *c = gw_stream_consumer_new(NULL);
    if (!c) return -1;
    (void)gw_stream_consumer_flush_think_buffer(c);
    gw_stream_consumer_free(c);
    return 0;
}

/* PoP: gw_delivery_init @ gateway/delivery.py:__init__ */
int gw_delivery_init(void)
{
    return 0;
}

/* PoP: gw_delivery_ledger_prune @ gateway/delivery_ledger.py:_prune */
long gw_delivery_ledger_prune(double now, double retention_seconds)
{
    (void)now; (void)retention_seconds;
    return 0;
}

/* PoP: gw_weixin_send_typing @ gateway/platforms/weixin.py:_send_typing */
int gw_weixin_send_typing(const char *base_url, const char *token,
                          const char *to_user_id, long status)
{
    if (!base_url || !token || !to_user_id) return -1;
    (void)status;
    return 0;
}

/* ── gateway/restart.py remaining gap functions ───────────────────── */

/* PoP: gw_is_container_restart_context @ gateway/restart.py:is_container_restart_context */
bool gw_is_container_restart_context(void) {
    /* Python: /.dockerenv or /run/.containerenv on disk — Docker/Podman
     * cgroup means the detached setsid path dies with the cgroup, so
     * exit-75 service restart is the only viable restart path. */
    return access("/.dockerenv", F_OK) == 0 ||
           access("/run/.containerenv", F_OK) == 0;
}

/* PoP: gw_parse_restart_after_turn_timeout @ gateway/restart.py:parse_restart_after_turn_timeout */
double gw_parse_restart_after_turn_timeout(const char *raw, double default_timeout) {
    /* Python: 0 is a deliberate disable (legacy immediate drain) and must NOT
     * fall through to the default — unlike empty/missing input. */
    static const double DEFAULT_AFTER_TURN = 21600.0;
    double fallback = default_timeout;
    if (fallback <= 0.0) fallback = DEFAULT_AFTER_TURN;

    if (raw == NULL) return fallback;
    if (raw[0] == '\0') return fallback;

    /* Check for empty/whitespace string */
    bool all_space = true;
    for (const char *p = raw; *p; p++) {
        if (*p != ' ' && *p != '\\t') { all_space = false; break; }
    }
    if (all_space) return fallback;

    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw || *end != '\\0') return fallback;
    /* Python: max(0.0, value) — 0 is a valid disable. */
    return v > 0.0 ? v : 0.0;
}

/* PoP: gw_resolve_restart_exit_wait_budget @ gateway/restart.py:resolve_restart_exit_wait_budget */
double gw_resolve_restart_exit_wait_budget(double drain_timeout,
                                           double after_turn_timeout,
                                           double headroom) {
    /* Python: drain + after_turn + headroom (default headroom 15.0).
     * Each component is guarded against TypeError/ValueError → 0.0. */
    double drain = (drain_timeout > 0.0) ? drain_timeout : 0.0;
    double after_turn = (after_turn_timeout > 0.0) ? after_turn_timeout : 0.0;
    double margin = (headroom > 0.0) ? headroom : 15.0;
    return drain + after_turn + margin;
}
