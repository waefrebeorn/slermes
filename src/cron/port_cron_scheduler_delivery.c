/*
 * port_cron_scheduler_delivery.c
 *
 * Faithful C11 port of the PURE cron delivery / origin / mirror / routing
 * helpers from cron/scheduler.py. See include/cron_scheduler_delivery.h.
 *
 * Every function here is deterministic and env/config-driven (no file lock,
 * no subprocess, no asyncio) so the logic is oracle-verifiable. The connected
 * platform set is passed in explicitly rather than loaded from the gateway so
 * the same logic can be exercised by the regression oracle.
 *
 * Module prefix used by the parity scanner for cron/scheduler.py is "scheduler_".
 */

#include "cron_scheduler_delivery.h"
#include "cron_scheduler_helpers.h"   /* scheduler_normalize_deliver_value */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Built-in tables (mirror the Python module constants) ───────────────── */

static const char *KNOWN_DELIVERY_PLATFORMS[] = {
    "telegram", "discord", "slack", "whatsapp", "signal",
    "matrix", "mattermost", "homeassistant", "dingtalk", "feishu",
    "wecom", "wecom_callback", "weixin", "sms", "email", "webhook",
    "bluebubbles", "qqbot", "yuanbao",
    NULL
};

/* platform -> home-channel env var (primary). Telegram thread override +
 * legacy QQ fallback handled in the accessor functions. */
static const struct { const char *platform; const char *env; } HOME_ENV_VARS[] = {
    { "matrix",      "MATRIX_HOME_ROOM" },
    { "telegram",    "TELEGRAM_HOME_CHANNEL" },
    { "discord",     "DISCORD_HOME_CHANNEL" },
    { "slack",       "SLACK_HOME_CHANNEL" },
    { "signal",      "SIGNAL_HOME_CHANNEL" },
    { "mattermost",  "MATTERMOST_HOME_CHANNEL" },
    { "sms",         "SMS_HOME_CHANNEL" },
    { "email",       "EMAIL_HOME_ADDRESS" },
    { "dingtalk",    "DINGTALK_HOME_CHANNEL" },
    { "feishu",      "FEISHU_HOME_CHANNEL" },
    { "wecom",       "WECOM_HOME_CHANNEL" },
    { "weixin",      "WEIXIN_HOME_CHANNEL" },
    { "bluebubbles", "BLUEBUBBLES_HOME_CHANNEL" },
    { "qqbot",       "QQBOT_HOME_CHANNEL" },
    { "whatsapp",    "WHATSAPP_HOME_CHANNEL" },
    { "whatsapp_cloud", "WHATSAPP_CLOUD_HOME_CHANNEL" },
    { NULL, NULL }
};

/* legacy env var names kept for back-compat (primary -> legacy). */
static const struct { const char *cur; const char *legacy; } LEGACY_HOME_ENV[] = {
    { "QQBOT_HOME_CHANNEL", "QQ_HOME_CHANNEL" },
    { NULL, NULL }
};

/* registry for plugin platforms (set via scheduler_register_plugin_platform). */
#define MAX_PLUGIN_PLATFORMS 32
static struct { char name[64]; char env[128]; int used; } g_plugin_platforms[MAX_PLUGIN_PLATFORMS];
static int g_plugin_count = 0;

/* ── small helpers ──────────────────────────────────────────────────────── */

static void lc(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; src[i] && i + 1 < n; i++) {
        char c = src[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        dst[i] = c;
    }
    dst[i] = '\0';
}

static int is_known_builtin(const char *name)
{
    char low[64];
    lc(low, name, sizeof(low));
    for (int i = 0; KNOWN_DELIVERY_PLATFORMS[i]; i++) {
        if (strcmp(low, KNOWN_DELIVERY_PLATFORMS[i]) == 0) return 1;
    }
    return 0;
}

static const char *builtin_home_env(const char *name)
{
    char low[64];
    lc(low, name, sizeof(low));
    for (int i = 0; HOME_ENV_VARS[i].platform; i++) {
        if (strcmp(low, HOME_ENV_VARS[i].platform) == 0)
            return HOME_ENV_VARS[i].env;
    }
    return NULL;
}

static const char *legacy_env_for(const char *cur)
{
    for (int i = 0; LEGACY_HOME_ENV[i].cur; i++) {
        if (strcmp(cur, LEGACY_HOME_ENV[i].cur) == 0)
            return LEGACY_HOME_ENV[i].legacy;
    }
    return NULL;
}

/* ── PoP: _resolve_origin @ cron/scheduler.py:_resolve_origin ───────────── */
/* PoP: scheduler_resolve_origin @ cron/scheduler.py:_resolve_origin */
int scheduler_resolve_origin(const scheduler_job_t *job, scheduler_origin_t *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!job) return 0;
    const scheduler_origin_t *o = &job->origin;
    if (!o->has_origin) return 0;
    if (!o->platform || !o->platform[0]) return 0;
    if (!o->chat_id || !o->chat_id[0]) return 0;
    if (out) {
        out->platform = o->platform;
        out->chat_id = o->chat_id;
        out->thread_id = o->thread_id;
        out->has_origin = 1;
    }
    return 1;
}

/* ── PoP: _cron_mirror_delivery_enabled @ cron/scheduler.py:_cron_mirror_delivery_enabled ── */
/* PoP: scheduler_cron_mirror_delivery_enabled @ cron/scheduler.py:_cron_mirror_delivery_enabled */
int scheduler_cron_mirror_delivery_enabled(const scheduler_job_t *job, int global_mirror)
{
    if (job && job->attach_to_session_present) {
        return job->attach_to_session_val ? 1 : 0;
    }
    return global_mirror ? 1 : 0;
}

/* ── PoP: _target_matches_origin @ cron/scheduler.py:_target_matches_origin ── */
/* PoP: scheduler_target_matches_origin @ cron/scheduler.py:_target_matches_origin */
int scheduler_target_matches_origin(const scheduler_origin_t *origin,
                                    const char *platform_name,
                                    const char *chat_id,
                                    const char *thread_id)
{
    if (!origin || !origin->has_origin) return 0;
    if (!platform_name || !chat_id) return 0;

    char opl[64], pnl[64];
    lc(opl, origin->platform ? origin->platform : "", sizeof(opl));
    lc(pnl, platform_name, sizeof(pnl));
    if (strcmp(opl, pnl) != 0) return 0;

    if (strcmp(origin->chat_id ? origin->chat_id : "", chat_id) != 0) return 0;

    const char *ot = origin->thread_id ? origin->thread_id : "";
    const char *tt = thread_id ? thread_id : "";
    /* origin pins a thread -> must match; otherwise any target thread matches */
    if (ot[0] != '\0' && strcmp(ot, tt) != 0) return 0;
    return 1;
}

/* ── PoP: _is_known_delivery_platform @ cron/scheduler.py:_is_known_delivery_platform ── */
/* PoP: scheduler_is_known_delivery_platform @ cron/scheduler.py:_is_known_delivery_platform */
int scheduler_is_known_delivery_platform(const char *name)
{
    if (!name || !name[0]) return 0;
    if (is_known_builtin(name)) return 1;
    /* plugin registry */
    char low[64];
    lc(low, name, sizeof(low));
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcmp(low, g_plugin_platforms[i].name) == 0) return 1;
    }
    return 0;
}

/* ── PoP: _resolve_home_env_var @ cron/scheduler.py:_resolve_home_env_var ── */
/* PoP: scheduler_resolve_home_env_var @ cron/scheduler.py:_resolve_home_env_var */
const char *scheduler_resolve_home_env_var(const char *name)
{
    if (!name || !name[0]) return NULL;
    const char *e = builtin_home_env(name);
    if (e) return e;
    char low[64];
    lc(low, name, sizeof(low));
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcmp(low, g_plugin_platforms[i].name) == 0)
            return g_plugin_platforms[i].env;
    }
    return NULL;
}

/* ── PoP: _get_home_target_chat_id @ cron/scheduler.py:_get_home_target_chat_id ── */
/* PoP: scheduler_get_home_target_chat_id @ cron/scheduler.py:_get_home_target_chat_id */
char *scheduler_get_home_target_chat_id(const char *name)
{
    const char *env = scheduler_resolve_home_env_var(name);
    if (!env) return strdup("");
    const char *v = getenv(env);
    if ((!v || !v[0]) && env) {
        const char *legacy = legacy_env_for(env);
        if (legacy) {
            const char *lv = getenv(legacy);
            if (lv && lv[0]) return strdup(lv);
        }
    }
    return strdup(v ? v : "");
}

/* ── PoP: _get_home_target_thread_id @ cron/scheduler.py:_get_home_target_thread_id ── */
/* PoP: scheduler_get_home_target_thread_id @ cron/scheduler.py:_get_home_target_thread_id */
char *scheduler_get_home_target_thread_id(const char *name)
{
    const char *env = scheduler_resolve_home_env_var(name);
    if (!env) { char *r = malloc(1); r[0] = '\0'; return r; }

    if (name && strcasecmp(name, "telegram") == 0) {
        const char *ct = getenv("TELEGRAM_CRON_THREAD_ID");
        if (ct && ct[0]) return strdup(ct);
    }
    /* read the THREAD_ID env (env var is the home-channel var, not the thread) */
    char thread_env[256];
    snprintf(thread_env, sizeof(thread_env), "%s_THREAD_ID", env);
    const char *v = getenv(thread_env);
    if ((!v || !v[0]) && env) {
        const char *legacy = legacy_env_for(env);
        if (legacy) {
            char legacy_thread_env[256];
            snprintf(legacy_thread_env, sizeof(legacy_thread_env), "%s_THREAD_ID", legacy);
            const char *lv = getenv(legacy_thread_env);
            if (lv && lv[0]) return strdup(lv);
        }
    }
    if (!v || !v[0]) { char *r = malloc(1); r[0] = '\0'; return r; }
    return strdup(v);
}

/* ── PoP: _iter_home_target_platforms @ cron/scheduler.py:_iter_home_target_platforms ── */
/* PoP: scheduler_iter_home_target_platforms @ cron/scheduler.py:_iter_home_target_platforms */
int scheduler_iter_home_target_platforms(const char **names_out, int max)
{
    int n = 0;
    for (int i = 0; HOME_ENV_VARS[i].platform && n < max; i++) {
        names_out[n++] = HOME_ENV_VARS[i].platform;
    }
    for (int i = 0; i < g_plugin_count && n < max; i++) {
        /* skip dup names already present (built-ins) */
        int dup = 0;
        for (int j = 0; j < n; j++) {
            if (strcasecmp(names_out[j], g_plugin_platforms[i].name) == 0) { dup = 1; break; }
        }
        if (!dup) names_out[n++] = g_plugin_platforms[i].name;
    }
    return n;
}

/* ── PoP: _expand_routing_tokens @ cron/scheduler.py:_expand_routing_tokens ──
 * "all" expands to every home-target platform with a configured home chat id
 * (env-driven on the C side). Unknown tokens pass through unchanged. */
/* PoP: scheduler_expand_routing_tokens @ cron/scheduler.py:_expand_routing_tokens */
int scheduler_expand_routing_tokens(const char *part, char **out_names, int max)
{
    if (!part || !part[0] || max <= 0) return 0;
    char low[64];
    lc(low, part, sizeof(low));
    if (strcmp(low, "all") != 0) {
        if (max > 0) out_names[0] = strdup(part);
        return max > 0 ? 1 : 0;
    }
    const char *names[64];
    int nn = scheduler_iter_home_target_platforms(names, 64);
    int n = 0;
    for (int i = 0; i < nn && n < max; i++) {
        char *cid = scheduler_get_home_target_chat_id(names[i]);
        int has = cid && cid[0];
        free(cid);
        if (has) out_names[n++] = strdup(names[i]);
    }
    return n;
}

/* helper: fill a target from platform name + origin when platform matches origin */
static int target_from_origin(const scheduler_origin_t *o, scheduler_target_t *out)
{
    memset(out, 0, sizeof(*out));
    strncpy(out->platform, o->platform ? o->platform : "", sizeof(out->platform) - 1);
    strncpy(out->chat_id, o->chat_id ? o->chat_id : "", sizeof(out->chat_id) - 1);
    if (o->thread_id && o->thread_id[0])
        strncpy(out->thread_id, o->thread_id, sizeof(out->thread_id) - 1);
    return 1;
}

/* ── PoP: _resolve_single_delivery_target @ cron/scheduler.py:_resolve_single_delivery_target ── */
/* PoP: scheduler_resolve_single_delivery_target @ cron/scheduler.py:_resolve_single_delivery_target */
int scheduler_resolve_single_delivery_target(const scheduler_job_t *job,
                                             const char *deliver_value,
                                             scheduler_target_t *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!job || !deliver_value || !deliver_value[0]) return 0;

    scheduler_origin_t origin;
    int has_origin = scheduler_resolve_origin(job, &origin);

    if (strcmp(deliver_value, "local") == 0) return 0;

    if (strcmp(deliver_value, "origin") == 0) {
        if (has_origin) { target_from_origin(&origin, out); return 1; }
        /* fallback: first home channel with a configured chat id */
        const char *names[64];
        int nn = scheduler_iter_home_target_platforms(names, 64);
        for (int i = 0; i < nn; i++) {
            char *cid = scheduler_get_home_target_chat_id(names[i]);
            int has = cid && cid[0];
            if (has) {
                char *tid = scheduler_get_home_target_thread_id(names[i]);
                strncpy(out->platform, names[i], sizeof(out->platform) - 1);
                strncpy(out->chat_id, cid, sizeof(out->chat_id) - 1);
                if (tid && tid[0]) strncpy(out->thread_id, tid, sizeof(out->thread_id) - 1);
                free(cid); free(tid);
                return 1;
            }
            free(cid);
        }
        return 0;
    }

    if (strchr(deliver_value, ':') != NULL) {
        char buf[512];
        strncpy(buf, deliver_value, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *colon = strchr(buf, ':');
        *colon = '\0';
        const char *platform_key = buf;          /* pre-colon, lowercased below */
        const char *rest = colon + 1;
        char pk[64];
        lc(pk, platform_key, sizeof(pk));
        /* Faithful behaviour: the Python path explicitly parses `rest` as
         * "chat_id" or "chat_id:thread_id" via _parse_target_ref, and resolves
         * human-friendly labels via resolve_channel_name. We replicate the
         * documented core: `rest` is "chat_id" or "chat_id:thread_id". */
        strncpy(out->platform, pk, sizeof(out->platform) - 1);
        char restbuf[512];
        strncpy(restbuf, rest, sizeof(restbuf) - 1);
        restbuf[sizeof(restbuf) - 1] = '\0';
        char *rcolon = strchr(restbuf, ':');
        if (rcolon) {
            *rcolon = '\0';
            strncpy(out->chat_id, restbuf, sizeof(out->chat_id) - 1);
            strncpy(out->thread_id, rcolon + 1, sizeof(out->thread_id) - 1);
        } else {
            strncpy(out->chat_id, restbuf, sizeof(out->chat_id) - 1);
        }
        return 1;
    }

    /* bare platform name */
    char low[64];
    lc(low, deliver_value, sizeof(low));
    if (has_origin && strcasecmp(origin.platform, deliver_value) == 0) {
        /* Python: prefer the platform's HOME channel when configured; only
         * fall back to the origin chat when no home target exists. */
        char *cid0 = scheduler_get_home_target_chat_id(deliver_value);
        if (cid0 && cid0[0]) {
            char *tid0 = scheduler_get_home_target_thread_id(deliver_value);
            strncpy(out->platform, deliver_value, sizeof(out->platform) - 1);
            strncpy(out->chat_id, cid0, sizeof(out->chat_id) - 1);
            if (tid0 && tid0[0]) strncpy(out->thread_id, tid0, sizeof(out->thread_id) - 1);
            free(cid0); free(tid0);
            return 1;
        }
        free(cid0);
        target_from_origin(&origin, out);
        /* Python keeps the caller-supplied platform name here, not origin's */
        memset(out->platform, 0, sizeof(out->platform));
        strncpy(out->platform, deliver_value, sizeof(out->platform) - 1);
        return 1;
    }
    if (!scheduler_is_known_delivery_platform(low)) return 0;
    char *cid = scheduler_get_home_target_chat_id(low);
    int has = cid && cid[0];
    if (!has) { free(cid); return 0; }
    char *tid = scheduler_get_home_target_thread_id(low);
    strncpy(out->platform, low, sizeof(out->platform) - 1);
    strncpy(out->chat_id, cid, sizeof(out->chat_id) - 1);
    if (tid && tid[0]) strncpy(out->thread_id, tid, sizeof(out->thread_id) - 1);
    free(cid); free(tid);
    return 1;
}

/* ── PoP: _resolve_delivery_targets @ cron/scheduler.py:_resolve_delivery_targets ──
 * csv deliver (+ "all" token), deduped by (platform, chat_id, thread_id). */
/* PoP: scheduler_resolve_delivery_target @ cron/scheduler.py:_resolve_delivery_target */
/* PoP: scheduler_resolve_delivery_targets @ cron/scheduler.py:_resolve_delivery_targets */
int scheduler_resolve_delivery_targets(const scheduler_job_t *job,
                                       scheduler_target_t *out, int max)
{
    if (!job || max <= 0) return 0;
    const char *deliver = scheduler_normalize_deliver_value(job->deliver ? job->deliver : "");
    if (strcmp(deliver, "local") == 0) { free((void *)deliver); return 0; }

    /* split csv */
    char *copy = strdup(deliver);
    free((void *)deliver);
    char *parts[64];
    int np = 0;
    char *p = copy;
    char *start = p;
    while (*p) {
        if (*p == ',') {
            *p = '\0';
            if (start[0]) parts[np++] = start;
            p++; start = p;
        } else p++;
    }
    if (start[0]) parts[np++] = start;

    scheduler_target_t t;
    int n = 0;
    for (int i = 0; i < np; i++) {
        char *expanded[64];
        int ne = scheduler_expand_routing_tokens(parts[i], expanded, 64);
        for (int e = 0; e < ne; e++) {
            if (scheduler_resolve_single_delivery_target(job, expanded[e], &t)) {
                int seen = 0;
                for (int k = 0; k < n; k++) {
                    if (strcasecmp(out[k].platform, t.platform) == 0 &&
                        strcmp(out[k].chat_id, t.chat_id) == 0 &&
                        strcmp(out[k].thread_id, t.thread_id) == 0) {
                        seen = 1; break;
                    }
                }
                if (!seen && n < max) {
                    memcpy(&out[n], &t, sizeof(t));
                    n++;
                }
            }
            free(expanded[e]);
        }
    }
    free(copy);
    return n;
}

/* ── PoP: _resolve_delivery_target @ cron/scheduler.py:_resolve_delivery_target ── */
int scheduler_resolve_delivery_target(const scheduler_job_t *job, scheduler_target_t *out)
{
    scheduler_target_t t;
    int n = scheduler_resolve_delivery_targets(job, &t, 1);
    if (n > 0 && out) memcpy(out, &t, sizeof(t));
    return n > 0 ? 1 : 0;
}

/* ── PoP: cron_delivery_targets @ cron/scheduler.py:cron_delivery_targets ──
 * Every home-target platform that is a known delivery platform AND in the
 * connected set, reporting whether its home channel is configured. */
/* PoP: scheduler_cron_delivery_targets @ cron/scheduler.py:cron_delivery_targets */
int scheduler_cron_delivery_targets(const char **connected, int n_connected,
                                    scheduler_delivery_desc_t *out, int max)
{
    if (max <= 0) return 0;
    const char *names[64];
    int nn = scheduler_iter_home_target_platforms(names, 64);
    int n = 0;
    for (int i = 0; i < nn && n < max; i++) {
        const char *nm = names[i];
        /* connected? */
        int connected_ok = 0;
        for (int c = 0; c < n_connected; c++) {
            if (strcasecmp(connected[c], nm) == 0) { connected_ok = 1; break; }
        }
        if (!connected_ok) continue;
        if (!scheduler_is_known_delivery_platform(nm)) continue;
        const char *env = scheduler_resolve_home_env_var(nm);
        char *cid = scheduler_get_home_target_chat_id(nm);
        int ht_set = cid && cid[0];
        memset(&out[n], 0, sizeof(out[n]));
        strncpy(out[n].id, nm, sizeof(out[n].id) - 1);
        /* name = name.replace("_"," ").title() */
        {
            char titled[128];
            size_t j = 0;
            int cap = 1;
            for (size_t k = 0; nm[k] && j + 1 < sizeof(titled); k++) {
                char c = nm[k];
                if (c == '_') { titled[j++] = ' '; cap = 1; continue; }
                if (cap && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
                titled[j++] = c;
                cap = 0;
            }
            titled[j] = '\0';
            strncpy(out[n].name, titled, sizeof(out[n].name) - 1);
        }
        out[n].home_target_set = ht_set ? 1 : 0;
        strncpy(out[n].home_env_var, env ? env : "", sizeof(out[n].home_env_var) - 1);
        free(cid);
        n++;
    }
    return n;
}

/* ── PoP: scheduler_plugin_env_var_lookup @ cron/scheduler.py:_plugin_cron_env_var ── */
/* Plugin-ONLY env-var lookup: built-ins never match here, exactly like the
 * Python accessor which only consults the plugin platform registry. Returns
 * the registered env var (static storage) or "" when not a plugin platform. */
const char *scheduler_plugin_env_var_lookup(const char *name)
{
    if (!name || !name[0]) return "";
    char low[64];
    lc(low, name, sizeof(low));
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcmp(low, g_plugin_platforms[i].name) == 0)
            return g_plugin_platforms[i].env;
    }
    return "";
}

/* ── plugin registry ──────────────────────────────────────────────────────── */
int scheduler_register_plugin_platform(const char *name, const char *env_var)
{
    if (!name || !name[0] || !env_var || !env_var[0]) return 0;
    char low[64];
    lc(low, name, sizeof(low));
    for (int i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugin_platforms[i].name, low) == 0) {
            strncpy(g_plugin_platforms[i].env, env_var,
                    sizeof(g_plugin_platforms[i].env) - 1);
            return 1;
        }
    }
    if (g_plugin_count >= MAX_PLUGIN_PLATFORMS) return 0;
    strncpy(g_plugin_platforms[g_plugin_count].name, low,
            sizeof(g_plugin_platforms[g_plugin_count].name) - 1);
    strncpy(g_plugin_platforms[g_plugin_count].env, env_var,
            sizeof(g_plugin_platforms[g_plugin_count].env) - 1);
    g_plugin_count++;
    return 1;
}
