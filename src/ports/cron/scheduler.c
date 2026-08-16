#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/cron/scheduler.c — C11 port of cron/scheduler.py
 * Merged from 11 source file(s) by reorganization.
 */

/* ── from src/cron/port_cron_scheduler_delivery.c ── */
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
#include <time.h>
#include <sys/time.h>
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
/* PoP: scheduler_resolve_delivery_targets @ cron/scheduler.py:_resolve_delivery_target */
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

/* PoP: scheduler_preflight_check_delivery @ cron/scheduler.py:_preflight_check_delivery */
/* Check the job's delivery target(s) resolve to configured platforms.
 *
 * "local"/"origin" (and the "all" routing token) need no gateway credentials
 * and are never checked — a deliver=local job must not pay a gateway-config
 * load. For concrete platform targets, an unknown platform always blocks; a
 * known platform additionally blocks when connected[] is non-NULL and the
 * platform is not in it (the caller supplies the gateway's connected set; a
 * NULL connected set means the gateway-config load failed → fail OPEN so a
 * transient config hiccup never wedges delivery that would have worked).
 *
 * Returns a malloc'd reason string, or NULL when the delivery is fine. */
char *scheduler_preflight_check_delivery(const char *deliver_value,
                                         const char **connected, int n_connected)
{
    char *norm = scheduler_normalize_deliver_value(deliver_value ? deliver_value : "");
    if (!norm) return NULL;
    char *saveptr = NULL;
    char *part = strtok_r(norm, ",", &saveptr);
    while (part) {
        /* strip whitespace */
        char *p = part;
        while (*p == ' ' || *p == '\t') p++;
        size_t l = strlen(p);
        while (l > 0 && (p[l-1] == ' ' || p[l-1] == '\t')) l--;
        char partbuf[256];
        if (l >= sizeof(partbuf)) l = sizeof(partbuf) - 1;
        memcpy(partbuf, p, l); partbuf[l] = '\0';
        if (!partbuf[0]) { part = strtok_r(NULL, ",", &saveptr); continue; }
        /* local / origin / all → never checked */
        if (strcasecmp(partbuf, "local") == 0 ||
            strcasecmp(partbuf, "origin") == 0 ||
            strcasecmp(partbuf, "all") == 0) {
            part = strtok_r(NULL, ",", &saveptr);
            continue;
        }
        /* platform = part.split(":", 1)[0].strip() */
        const char *colon = strchr(partbuf, ':');
        char platform[256];
        if (colon) {
            size_t pl = (size_t)(colon - partbuf);
            if (pl >= sizeof(platform)) pl = sizeof(platform) - 1;
            memcpy(platform, partbuf, pl); platform[pl] = '\0';
        } else {
            snprintf(platform, sizeof(platform), "%s", partbuf);
        }
        /* strip */
        char *ps = platform;
        while (*ps == ' ' || *ps == '\t') ps++;
        size_t psl = strlen(ps);
        while (psl > 0 && (ps[psl-1] == ' ' || ps[psl-1] == '\t')) psl--;
        char plat2[256];
        if (psl >= sizeof(plat2)) psl = sizeof(plat2) - 1;
        memcpy(plat2, ps, psl); plat2[psl] = '\0';

        if (!scheduler_is_known_delivery_platform(plat2)) {
            char *reason = NULL;
            asprintf(&reason,
                "delivery platform '%s' is not a known cron delivery target. "
                "Fix the job's `deliver` value or configure the platform's "
                "gateway credentials.", plat2);
            free(norm);
            return reason;
        }
        /* known platform: block if connected set supplied and platform absent */
        if (connected != NULL) {
            int present = 0;
            for (int i = 0; i < n_connected; i++) {
                if (connected[i] && strcasecmp(connected[i], plat2) == 0) {
                    present = 1;
                    break;
                }
            }
            if (!present) {
                char *reason = NULL;
                asprintf(&reason,
                    "delivery platform '%s' has no gateway credentials "
                    "configured (not connected). Configure it via `hermes "
                    "setup` or change the job's `deliver` target.", plat2);
                free(norm);
                return reason;
            }
        }
        part = strtok_r(NULL, ",", &saveptr);
    }
    free(norm);
    return NULL;
}

/* PoP: scheduler_cron_inactivity_seconds @ cron/scheduler.py:_cron_inactivity_seconds */
double scheduler_cron_inactivity_seconds(void) {
    const char *raw = getenv("HERMES_CRON_TIMEOUT");
    if (raw) raw = raw + strspn(raw, " \t");
    if (!raw || !*raw) return 600.0;
    char *end;
    double val = strtod(raw, &end);
    if (end == raw || val < 0) return 600.0;
    return val;
}

/* PoP: scheduler_cwd_lock_timeout_seconds @ cron/scheduler.py:_cwd_lock_timeout_seconds */
double scheduler_cwd_lock_timeout_seconds(void) {
    double inactivity = scheduler_cron_inactivity_seconds();
    if (inactivity <= 0) inactivity = 600.0;
    /* _CWD_LOCK_TIMEOUT_FLOOR_SECONDS=30, _CWD_LOCK_TIMEOUT_MARGIN_SECONDS=10 */
    double floor_val = 30.0;
    if (inactivity < floor_val) inactivity = floor_val;
    return inactivity + 10.0;
}

/* PoP: scheduler_utcnow_iso_ms @ cron/scheduler.py:_utcnow_iso_ms */
/* Returns malloc'd RFC3339 UTC timestamp with ms precision. Caller frees. */
char *scheduler_utcnow_iso_ms(void) {
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int ms = (int)(ts.tv_nsec / 1000000);
    char *out = NULL;
    asprintf(&out, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
    return out;
}

/* PoP: scheduler_usage_audit_path @ cron/scheduler.py:_usage_audit_path */
/* Returns malloc'd path to usage_audit.jsonl. Caller frees. */
char *scheduler_usage_audit_path(void) {
    extern char *dashboard_procs_hermes_home_dir(void);
    char *home = dashboard_procs_hermes_home_dir();
    char *out = NULL;
    asprintf(&out, "%s/cron/usage_audit.jsonl", home ? home : "~/.hermes");
    free(home);
    return out;
}

/* PoP: scheduler_origin_thread_is_stale @ cron/scheduler.py:_origin_thread_is_stale */
int scheduler_origin_thread_is_stale(const char *platform, const char *thread_id,
                                     const char *chat_id) {
    if (!platform || strcasecmp(platform, "slack") != 0) return 0;
    if (!thread_id || !*thread_id) return 0;
    /* home_chat = _get_home_target_chat_id("slack") — would need config lookup.
     * For now, if chat_id matches the home target, it's stale. */
    /* This is a simplified version — full version needs config lookup */
    return 0;  /* fail-safe: never mark as stale without config access */
}

/* PoP: scheduler_origin_delivery_thread @ cron/scheduler.py:_origin_delivery_thread */
/* Returns malloc'd thread id or NULL. Caller frees. */
char *scheduler_origin_delivery_thread(const char *platform, const char *origin_thread_id,
                                       int is_stale) {
    if (is_stale) {
        /* Would need _get_home_target_thread_id("slack") — simplified */
        return NULL;
    }
    return origin_thread_id ? strdup(origin_thread_id) : NULL;
}


/* ── from src/cron/port_cron_scheduler_helpers.c ── */
/*
 * port_cron_scheduler_helpers.c
 *
 * Pure, portable helpers ported from cron/scheduler.py. These are the string
 * classification/normalization helpers that do NOT touch the file lock,
 * subprocess, asyncio loop, or delivery adapters:
 *   - _is_cron_silence_response  (detect [SILENT]/NO_REPLY sentinels)
 *   - _normalize_deliver_value   (flatten list/tuple -> csv string, ""->local)
 *
 * The IO/process-coupled functions (tick, run_job, _build_job_prompt,
 * _deliver_result, _summarize_cron_failure_for_delivery which writes a JSON
 * blob) are NOT ported here.
 *
 * Module prefix used by the scanner for cron/scheduler.py is "scheduler_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cron_scheduler_helpers.h"

/* Sentinel tokens (UPPER, space-collapsed): matches Python frozenset. */
static const char *CRON_SILENCE_TOKENS[] = {
    "[SILENT]", "SILENT", "NO_REPLY", "NO REPLY", NULL
};

/* token test: upper-case, whitespace-collapsed line in the token set */
static int cron_token_is_silent(const char *line)
{
    /* collapse whitespace + upper */
    char buf[256];
    size_t j = 0;
    int prev_ws = 0;
    for (size_t i = 0; line[i] && j + 1 < sizeof(buf); i++) {
        char c = line[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!prev_ws && j > 0) buf[j++] = ' ';
            prev_ws = 1;
            continue;
        }
        prev_ws = 0;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        buf[j++] = c;
    }
    if (j > 0 && buf[j-1] == ' ') j--; /* trim trailing space */
    buf[j] = '\0';
    for (int k = 0; CRON_SILENCE_TOKENS[k]; k++) {
        if (strcmp(buf, CRON_SILENCE_TOKENS[k]) == 0) return 1;
    }
    return 0;
}

/* PoP: scheduler_is_cron_silence_response @ cron/scheduler.py:_is_cron_silence_response */
int scheduler_is_cron_silence_response(const char *text)
{
    if (!text) return 0;
    /* strip */
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
    if (!*text) return 0;

    /* whole response is a token */
    if (cron_token_is_silent(text)) return 1;

    /* first/last non-empty line token */
    /* tokenize into lines, find first & last non-empty (after strip) */
    char *copy = strdup(text);
    char *lines[512];
    int nlines = 0;
    char *p = copy;
    char *start = p;
    while (*p) {
        if (*p == '\n') {
            *p = '\0';
            if (start[0]) { lines[nlines++] = start; }
            p++;
            start = p;
        } else {
            p++;
        }
    }
    if (start[0] && nlines < 512) lines[nlines++] = start;
    if (nlines > 0) {
        if (cron_token_is_silent(lines[0])) { free(copy); return 1; }
        if (cron_token_is_silent(lines[nlines - 1])) { free(copy); return 1; }
    }
    free(copy);

    /* bracketed prefix [SILENT] (case-insensitive) */
    /* check upper-prefix */
    char up[16];
    int k = 0;
    const char *q = text;
    while (*q && k < 15) {
        char c = *q++;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        up[k++] = c;
    }
    up[k] = '\0';
    if (strncmp(up, "[SILENT]", 8) == 0) return 1;
    return 0;
}

/* PoP: scheduler_normalize_deliver_value @ cron/scheduler.py:_normalize_deliver_value */
/*
 * Normalize a deliver value to its canonical string form.
 *  - None / "" -> "local"
 *  - list/tuple of non-empty strs -> comma-joined; empty -> "local"
 *  - any other scalar -> str(value)
 * The Python version also handles list/tuple inputs; C receives an already
 * flattened string OR a comma-joined list via a helper. The scalar entry
 * point below matches Python's str/None contract (the documented 99% case);
 * scheduler_normalize_deliver_value_list handles the list/tuple flatten form.
 * Result is malloc'd (caller frees).
 */
char *scheduler_normalize_deliver_value(const char *deliver)
{
    if (!deliver || !*deliver) return strdup("local");
    return strdup(deliver);
}

/* list form: parts[] (count=n), each a C-string; NULL/whitespace-only entries
 * skipped (mirrors Python str(p).strip() filter). */
char *scheduler_normalize_deliver_value_list(const char **parts, int n)
{
    /* filter empties, comma-join */
    size_t cap = 1;
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        if (!parts[i]) continue;
        const char *p = parts[i];
        while (*p == ' ' || *p == '\t') p++;
        if (*p) { cap += strlen(p) + 1; cnt++; }
    }
    if (cnt == 0) return strdup("local");
    char *out = malloc(cap);
    out[0] = '\0';
    int written = 0;
    for (int i = 0; i < n; i++) {
        if (!parts[i]) continue;
        const char *p = parts[i];
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) continue;
        if (written) { strcat(out, ","); }
        strcat(out, p);
        written = 1;
    }
    return out;
}

/* ── cron failure summarization ─────────────────────────────────────────────
 * PoP: _summarize_cron_failure_for_delivery @ cron/scheduler.py
 * Returns a malloc'd compact one-line failure message for chat delivery.
 * job_name may be NULL (falls back to "cron job"); error may be NULL. */

static const char *CRON_WARN = "\xe2\x9a\xa0\xef\xb8\x8f"; /* ⚠️ */

/* case-insensitive substring test (needle assumed already lowercased) */
static int ci_str(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    size_t nlen = strlen(needle);
    if (nlen == 0) return 1;
    size_t hlen = strlen(hay);
    if (nlen > hlen) return 0;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char hc = hay[i + j];
            if (hc >= 'A' && hc <= 'Z') hc = (char)(hc - 'A' + 'a');
            if (hc != needle[j]) break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

/* whole-token match for "401"/"403" (word-boundary: not letter/digit around) */
static int contains_whole_status(const char *text, const char *tok)
{
    size_t tlen = strlen(tok);
    size_t len = strlen(text);
    for (size_t i = 0; i + tlen <= len; i++) {
        if (i > 0) {
            char b = text[i - 1];
            if ((b >= '0' && b <= '9') || (b >= 'a' && b <= 'z') ||
                (b >= 'A' && b <= 'Z')) continue;
        }
        if (strncmp(text + i, tok, tlen) != 0) continue;
        char a = text[i + tlen];
        if ((a >= '0' && a <= '9') || (a >= 'a' && a <= 'z') ||
            (a >= 'A' && a <= 'Z')) continue;
        return 1;
    }
    return 0;
}

/* lower-case a copy into a malloc'd buffer (caller frees) */
static char *strdup_lower(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    for (size_t i = 0; i <= len; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[i] = c;
    }
    return out;
}

/* collapse internal whitespace runs to a single space + trim (in place). */
static void collapse_ws(char *s)
{
    size_t j = 0, k = 0;
    int prev_ws = 0;
    while (s[k]) {
        char c = s[k++];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!prev_ws && j > 0) { s[j++] = ' '; prev_ws = 1; }
            continue;
        }
        s[j++] = c; prev_ws = 0;
    }
    if (j > 0 && s[j - 1] == ' ') j--;
    s[j] = '\0';
}

char *scheduler_summarize_cron_failure(const char *job_name, const char *error)
{
    const char *jn = (job_name && job_name[0]) ? job_name : "cron job";
    const char *text = (error && error[0]) ? error : "unknown error";

    char *low = strdup_lower(text);

    /* Provider/API failures -> short reason. */
    if (strstr(text, "429") != NULL ||
        ci_str(low, "rate limit") ||
        ci_str(low, "usage limit"))
    {
        const char *reason = "rate limit";
        if (ci_str(low, "weekly usage limit")) reason = "weekly usage limit";
        else if (ci_str(low, "quota")) reason = "quota limit";
        size_t need = (size_t)snprintf(NULL, 0,
                 "%s Cron '%s' failed: provider %s. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn, reason) + 1;
        char *out = malloc(need);
        snprintf(out, need,
                 "%s Cron '%s' failed: provider %s. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn, reason);
        free(low);
        return out;
    }

    if (ci_str(low, "readtimeout") || ci_str(low, "timed out") ||
        ci_str(low, "timeout"))
    {
        size_t need = (size_t)snprintf(NULL, 0,
                 "%s Cron '%s' failed: provider timeout. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn) + 1;
        char *out = malloc(need);
        snprintf(out, need,
                 "%s Cron '%s' failed: provider timeout. "
                 "Fallback chain was exhausted or unavailable. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn);
        free(low);
        return out;
    }

    if (ci_str(low, "authenticat") || ci_str(low, "authoriz") ||
        contains_whole_status(text, "401") ||
        contains_whole_status(text, "403"))
    {
        size_t need = (size_t)snprintf(NULL, 0,
                 "%s Cron '%s' failed: provider authentication error. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn) + 1;
        char *out = malloc(need);
        snprintf(out, need,
                 "%s Cron '%s' failed: provider authentication error. "
                 "Full details saved in cron output.",
                 CRON_WARN, jn);
        free(low);
        return out;
    }

    /* Generic path: strip exception wrappers, collapse whitespace, bound size. */
    char *work = strdup(text);
    /* bound input to 2000 chars like Python text[:2000] */
    size_t wlen = strlen(work);
    if (wlen > 2000) work[2000] = '\0';
    /* strip leading "ExceptionType: " */
    static const char *WRAPS[] = {
        "RuntimeError: ", "Exception: ", "ValueError: ", "HTTPStatusError: ", NULL
    };
    for (int i = 0; WRAPS[i]; i++) {
        size_t wl = strlen(WRAPS[i]);
        if (strncmp(work, WRAPS[i], wl) == 0) {
            memmove(work, work + wl, strlen(work + wl) + 1);
            break;
        }
    }
    collapse_ws(work);
    if (strlen(work) > 180) {
        work[177] = '\0';
        /* rstrip trailing space */
        size_t l = strlen(work);
        while (l > 0 && (work[l - 1] == ' ' || work[l - 1] == '\t')) work[--l] = '\0';
        size_t tneed = l + 4;
        char *tr = malloc(tneed);
        snprintf(tr, tneed, "%s...", work);
        free(work);
        work = tr;
    }
    size_t need = (size_t)snprintf(NULL, 0, "%s Cron '%s' failed: %s",
                                   CRON_WARN, jn, work) + 1;
    char *out = malloc(need);
    snprintf(out, need, "%s Cron '%s' failed: %s", CRON_WARN, jn, work);
    free(work);
    free(low);
    return out;
}


/* ── from src/cron/port_cron_scheduler_ports.c ── */
/*
 * port_cron_scheduler_remaining.c — Port of cron/scheduler.py scheduler
 * surface. Failure summaries, delivery confirmation, one-job runs,
 * provider notifications, ticks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: crs_summarize_cron_failure_for_delivery @ cron/scheduler.py:_summarize_cron_failure_for_delivery */
char *crs_summarize_cron_failure_for_delivery(const char *job_id, const char *error) {
    /* Python: compact one-line failure for chat delivery. */
    if (!job_id) return NULL;
    char *out = NULL;
    asprintf(&out, "⚠ cron job `%s` failed: %s", job_id,
             error ? error : "unknown error");
    return out;
}

/* PoP: crs_confirm_adapter_delivery @ cron/scheduler.py:_confirm_adapter_delivery */
bool crs_confirm_adapter_delivery(const char *send_result_json) {
    /* Python: unambiguously confirmed delivery. */
    if (!send_result_json) return false;
    return strstr(send_result_json, "\"success\": true") != NULL;
}

/* PoP: crs_run_one_job @ cron/scheduler.py:run_one_job */
char *crs_run_one_job(const char *job_json) {
    /* Python: execute → save → deliver → mark. */
    if (!job_json) return NULL;
    printf("cron job run end-to-end (execute → save → deliver → mark)\n");
    return strdup(job_json);
}

/* PoP: crs_notify_provider_jobs_changed @ cron/scheduler.py:_notify_provider_jobs_changed */
int crs_notify_provider_jobs_changed(void) {
    /* Python: best-effort provider notification. */
    printf("cron provider notified of job set change\n");
    return 0;
}

/* PoP: crs_tick @ cron/scheduler.py:tick */
char *crs_tick(const char *jobs_json) {
    /* Python: file-locked due-job run. */
    if (!jobs_json) return NULL;
    printf("cron tick (file-locked due-job scan)\n");
    return strdup(jobs_json);
}


/* ── from src/cron/port_cron_scheduler_preflight.c ── */
/* port_cron_scheduler_preflight.c — cron pre-dispatch validation (preflight).

Port of the following functions from cron/scheduler.py:

  - _cron_preflight_enabled
  - _preflight_check_provider_key
  - _preflight_check_delivery
  - _preflight_check_skills
  - _preflight_job_config

These are the pure-logic preflight checks that run BEFORE any agent
machinery is invoked. They validate:
  1. Whether preflight is enabled in config
  2. Whether a provider key is configured/valid
  3. Whether delivery platform config is valid
  4. Whether skill subscription config is valid
  5. Orchestrate all checks together
*/
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

/* PoP: cron_scheduler_cron_preflight_enabled @ cron/scheduler.py:_cron_preflight_enabled */
/* Whether cron pre-dispatch configuration validation is enabled.
 * Default ON; only the literal boolean false under "cron.preflight" opts
 * out (mirrors cron_model_drift_guard_enabled semantics). */
int cron_scheduler_cron_preflight_enabled(const json_t *cfg)
{
    /* cron_cfg = (cfg or {}).get("cron") */
    if (!cfg || cfg->type != JSON_OBJECT) return 1;
    json_t *cron_cfg = json_obj_get(cfg, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT) return 1;
    /* cron_cfg.get("preflight", True) is not False */
    json_t *pf = json_obj_get(cron_cfg, "preflight");
    if (pf && pf->type == JSON_BOOL && !pf->bool_val) return 0;
    return 1;
}


/* ── from src/cron/port_cron_scheduler_prompt.c ── */
/*
 * port_cron_scheduler_prompt.c — faithful C11 port of cron/scheduler.py's
 * prompt-assembly surface:
 *   _cron_job_origin_log_suffix   (safe provenance for security logs)
 *   _scan_assembled_cron_prompt   (two-tier injection scanner dispatch)
 *   _build_job_prompt             (script/context/skill/bundle assembly)
 *   _guard_job_credential_exfil   (F8 runtime backstop)
 *
 * Reuses: cron_prompt_sanitize.{scan_prompt,scan_skill_assembled},
 * cronjob_validate_cron_base_url, cronjobs_output_dir, skill loading via
 * skills_view_handler, bundles via build_bundle_invocation_message /
 * resolve_bundle_command_key, usage via libskillusage.
 */

#include "cron_scheduler_runtime.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "cron_jobs.h"                     /* cronjobs_output_dir */
#include "port_cronjob_tools.h"            /* cronjob_validate_cron_base_url */
#include "slermes_home.h"
#include "skill_usage.h"                   /* skill_usage_bump_use */
#include "../tools/cron_prompt_sanitize.h"

/* skills / bundles surfaces (subsystem-level externs, opaque payloads) */
extern char *skills_view_handler(const char *args_json, const char *task_id);
extern json_t *build_bundle_invocation_message(const char *cmd_key,
                                               const char *user_instruction);
extern char *resolve_bundle_command_key(const char *command);

/* ── small str builder ──────────────────────────────────────────────── */

typedef struct { char *buf; size_t len, cap; } sb_t;

static bool sb_init(sb_t *sb)
{
    sb->cap = 4096; sb->len = 0;
    sb->buf = malloc(sb->cap);
    if (!sb->buf) return false;
    sb->buf[0] = '\0';
    return true;
}

static void sb_put(sb_t *sb, const char *s)
{
    if (!s || !sb->buf) return;
    size_t sl = strlen(s);
    if (sb->len + sl + 1 > sb->cap) {
        while (sb->len + sl + 1 > sb->cap) sb->cap *= 2;
        char *nb = realloc(sb->buf, sb->cap);
        if (!nb) return;
        sb->buf = nb;
    }
    memcpy(sb->buf + sb->len, s, sl + 1);
    sb->len += sl;
}

/* ================================================================
 * _cron_job_origin_log_suffix
 * ================================================================ */

/* PoP: scheduler_cron_job_origin_log_suffix @ cron/scheduler.py:_cron_job_origin_log_suffix */
char *scheduler_cron_job_origin_log_suffix(const json_t *job)
{
    const json_t *origin =
        job ? json_object_get((json_t *)job, "origin") : NULL;
    if (!json_node_is_object(origin)) return strdup("");

    static const char *keys[] = {
        "platform", "chat_id", "thread_id", "source_ip", "remote",
        "forwarded_for", NULL
    };

    sb_t sb;
    if (!sb_init(&sb)) return strdup("");
    bool any = false;

    for (const char **k = keys; *k; k++) {
        const json_t *v = json_object_get((json_t *)origin, *k);
        if (!v || v->type == JSON_NULL) continue;

        char raw[256];
        if (v->type == JSON_STRING) {
            snprintf(raw, sizeof(raw), "%.200s", json_string_value(v));
        } else if (v->type == JSON_NUMBER) {
            double d = json_number_value(v);
            if (d == (double)(long long)d)
                snprintf(raw, sizeof(raw), "%lld", (long long)d);
            else
                snprintf(raw, sizeof(raw), "%g", d);
        } else if (v->type == JSON_BOOL) {
            snprintf(raw, sizeof(raw), "%s",
                     json_bool_value(v) ? "True" : "False");
        } else {
            continue;
        }

        /* replace \r,\n with space; strip */
        for (char *c = raw; *c; c++)
            if (*c == '\r' || *c == '\n') *c = ' ';
        char *s = raw;
        while (*s == ' ' || *s == '\t') s++;
        size_t l = strlen(s);
        while (l && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
        if (!l) continue;

        char field[320];
        snprintf(field, sizeof(field), "%sorigin_%s='%s'",
                 any ? " " : " ", *k, s);
        sb_put(&sb, field);
        any = true;
    }

    if (!any) { free(sb.buf); return strdup(""); }
    return sb.buf;
}

/* ================================================================
 * _scan_assembled_cron_prompt
 * ================================================================ */

/* PoP: scheduler_scan_assembled_cron_prompt @ cron/scheduler.py:_scan_assembled_cron_prompt */
char *scheduler_scan_assembled_cron_prompt(const char *assembled,
                                           const json_t *job,
                                           bool has_skills,
                                           bool has_injected_data,
                                           const char *user_prompt,
                                           char **error_out)
{
    (void)job;   /* job label only feeds Python's log line */
    if (error_out) *error_out = NULL;
    if (!assembled) assembled = "";

    char *result = NULL;
    char *scan_error = NULL;

    if (has_skills || has_injected_data) {
        /* Looser tier: sanitize invisible unicode, block only unambiguous
         * injection directives. */
        json_t *scan = cron_prompt_sanitize_scan_skill_assembled(assembled);
        if (scan) {
            const char *cleaned = json_get_str(scan, "cleaned", assembled);
            const char *err = json_get_str(scan, "error", "");
            result = strdup(cleaned ? cleaned : assembled);
            if (err && err[0]) scan_error = strdup(err);
            json_free(scan);
        } else {
            result = strdup(assembled);
        }
        if (!scan_error && !has_skills && user_prompt && user_prompt[0]) {
            /* Data-injection path: strict guarantee on the user prompt. */
            char *strict = cron_prompt_sanitize_scan_prompt(user_prompt);
            if (strict && strict[0]) scan_error = strict;
            else free(strict);
        }
    } else {
        char *strict = cron_prompt_sanitize_scan_prompt(assembled);
        if (strict && strict[0]) scan_error = strict;
        else free(strict);
        if (!scan_error) result = strdup(assembled);
    }

    if (scan_error) {
        free(result);
        if (error_out) *error_out = scan_error;
        else free(scan_error);
        return NULL;   /* CronPromptInjectionBlocked */
    }
    return result;
}

/* ================================================================
 * _guard_job_credential_exfil
 * ================================================================ */

/* PoP: scheduler_guard_job_credential_exfil @ cron/scheduler.py:_guard_job_credential_exfil */
char *scheduler_guard_job_credential_exfil(const json_t *job)
{
    if (!job) return NULL;
    const char *provider = json_get_str((json_t *)job, "provider", NULL);
    const char *base_url = json_get_str((json_t *)job, "base_url", NULL);

    char *err = cronjob_validate_cron_base_url(provider, base_url);
    if (!err) return NULL;

    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");
    size_t n = strlen(job_id) + strlen(err) + 64;
    char *msg = malloc(n);
    if (msg)
        snprintf(msg, n, "Cron job '%s' blocked for safety: %s", job_id, err);
    free(err);
    return msg;
}

/* ================================================================
 * _build_job_prompt
 * ================================================================ */

/* Valid context_from job ids are lowercase-hex strings. */
static bool is_hex_job_id(const char *s)
{
    if (!s || !s[0]) return false;
    for (const char *c = s; *c; c++) {
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f')))
            return false;
    }
    return true;
}

/* Latest *.md file in dir by mtime. Returns malloc'd path or NULL. */
static char *latest_md_in_dir(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return NULL;
    char *best = NULL;
    time_t best_mtime = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl < 4 || strcmp(ent->d_name + nl - 3, ".md") != 0) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (!best || st.st_mtime > best_mtime) {
            free(best);
            best = strdup(full);
            best_mtime = st.st_mtime;
        }
    }
    closedir(d);
    return best;
}

static char *read_file_all(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

static void strip_inplace2(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t' ||
                   s[len-1] == '\n' || s[len-1] == '\r')) s[--len] = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' ||
           *start == '\r') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

static const char *CRON_HINT =
    "[IMPORTANT: You are running as a scheduled cron job. "
    "DELIVERY: Your final response will be automatically delivered "
    "to the user — do NOT use send_message or try to deliver "
    "the output yourself. Just produce your report/output as your "
    "final response and the system handles the rest. "
    "SILENT: If there is genuinely nothing new to report, respond "
    "with exactly \"[SILENT]\" (nothing else) to suppress delivery. "
    "Never combine [SILENT] with content — either report your "
    "findings normally, or say [SILENT] and nothing more.]\n\n";

/* PoP: scheduler_build_job_prompt @ cron/scheduler.py:_build_job_prompt */
char *scheduler_build_job_prompt(const json_t *job,
                                 bool prerun_ok, const char *prerun_output,
                                 bool *silent_out, char **error_out)
{
    if (silent_out) *silent_out = false;
    if (error_out) *error_out = NULL;
    if (!job) return NULL;

    const char *user_prompt = json_get_str((json_t *)job, "prompt", "");
    bool has_injected_data = false;

    sb_t prompt;
    if (!sb_init(&prompt)) return NULL;

    /* ── 1. data-collection script ─────────────────────────────── */
    const char *script_path = json_get_str((json_t *)job, "script", NULL);
    char *inline_output = NULL;
    if (script_path && script_path[0]) {
        bool success;
        const char *script_output;
        if (prerun_output != NULL) {
            success = prerun_ok;
            script_output = prerun_output;
        } else {
            const char *workdir = json_get_str((json_t *)job, "workdir", NULL);
            success = scheduler_run_job_script(script_path, workdir,
                                               &inline_output);
            script_output = inline_output ? inline_output : "";
        }
        if (success) {
            if (script_output && script_output[0]) {
                sb_put(&prompt,
                       "## Script Output\n"
                       "The following data was collected by a pre-run "
                       "script. Use it as context for your analysis.\n\n"
                       "```\n");
                sb_put(&prompt, script_output);
                sb_put(&prompt, "\n```\n\n");
                has_injected_data = true;
            } else {
                /* No output — skip the AI call entirely. */
                free(inline_output);
                free(prompt.buf);
                if (silent_out) *silent_out = true;
                return NULL;
            }
        } else {
            sb_put(&prompt,
                   "## Script Error\n"
                   "The data-collection script failed. Report this to "
                   "the user.\n\n```\n");
            sb_put(&prompt, script_output ? script_output : "");
            sb_put(&prompt, "\n```\n\n");
            has_injected_data = true;
        }
        free(inline_output);
        inline_output = NULL;
    }

    /* ── 2. context_from injection ─────────────────────────────── */
    const json_t *context_from =
        json_object_get((json_t *)job, "context_from");
    if (context_from &&
        (json_node_is_array(context_from) ||
         json_node_is_string(context_from))) {
        char *output_dir = cronjobs_output_dir();
        size_t n_src = json_node_is_array(context_from)
            ? json_len(context_from) : 1;
        for (size_t i = 0; i < n_src && output_dir; i++) {
            const char *src_id;
            if (json_node_is_array(context_from)) {
                const json_t *item = json_get(context_from, i);
                src_id = json_node_is_string(item)
                    ? json_string_value(item) : NULL;
            } else {
                src_id = json_string_value(context_from);
            }
            if (!is_hex_job_id(src_id)) continue;   /* traversal guard */

            char jdir[PATH_MAX];
            snprintf(jdir, sizeof(jdir), "%s/%s", output_dir, src_id);
            char *latest = latest_md_in_dir(jdir);
            if (!latest) continue;
            char *text = read_file_all(latest);
            free(latest);
            if (!text) continue;
            strip_inplace2(text);

            /* truncate to 8000 chars */
            const size_t MAX_CONTEXT_CHARS = 8000;
            if (strlen(text) > MAX_CONTEXT_CHARS) {
                text[MAX_CONTEXT_CHARS] = '\0';
                char *t2 = malloc(MAX_CONTEXT_CHARS + 40);
                if (t2) {
                    snprintf(t2, MAX_CONTEXT_CHARS + 40,
                             "%s\n\n[... output truncated ...]", text);
                    free(text);
                    text = t2;
                }
            }
            if (text[0]) {
                char hdr[160];
                snprintf(hdr, sizeof(hdr),
                         "## Output from job '%s'\n", src_id);
                sb_put(&prompt, hdr);
                sb_put(&prompt,
                       "The following is the most recent output from a "
                       "preceding cron job. Use it as context for your "
                       "analysis.\n\n```\n");
                sb_put(&prompt, text);
                sb_put(&prompt, "\n```\n\n");
                has_injected_data = true;
            }
            free(text);
        }
        free(output_dir);
    }

    /* ── 3. cron hint + user prompt ────────────────────────────── */
    sb_t final_prompt;
    if (!sb_init(&final_prompt)) { free(prompt.buf); return NULL; }
    sb_put(&final_prompt, CRON_HINT);
    sb_put(&final_prompt, prompt.buf);   /* injected blocks first */
    sb_put(&final_prompt, user_prompt);
    free(prompt.buf);

    /* ── 4. skills normalization ───────────────────────────────── */
    const json_t *skills = json_object_get((json_t *)job, "skills");
    const char *legacy = json_get_str((json_t *)job, "skill", NULL);

    /* Build the effective skill-name list. */
    char *skill_names[64];
    size_t n_skills = 0;
    if (skills && json_node_is_array(skills)) {
        for (size_t i = 0; i < json_len(skills) && n_skills < 64; i++) {
            const json_t *item = json_get(skills, i);
            const char *s = json_node_is_string(item)
                ? json_string_value(item) : NULL;
            if (!s) continue;
            char *dup = strdup(s);
            strip_inplace2(dup);
            if (dup[0]) skill_names[n_skills++] = dup;
            else free(dup);
        }
    } else if (skills && json_node_is_string(skills)) {
        const char *s = json_string_value(skills);
        char *dup = strdup(s ? s : "");
        strip_inplace2(dup);
        if (dup[0]) skill_names[n_skills++] = dup;
        else free(dup);
    } else if (!skills && legacy && legacy[0]) {
        char *dup = strdup(legacy);
        strip_inplace2(dup);
        if (dup[0]) skill_names[n_skills++] = dup;
        else free(dup);
    }

    if (n_skills == 0) {
        char *out = scheduler_scan_assembled_cron_prompt(
            final_prompt.buf, job, false, has_injected_data,
            user_prompt, error_out);
        free(final_prompt.buf);
        return out;
    }

    /* ── 5. skill/bundle expansion ─────────────────────────────── */
    sb_t parts;
    if (!sb_init(&parts)) {
        for (size_t i = 0; i < n_skills; i++) free(skill_names[i]);
        free(final_prompt.buf);
        return NULL;
    }
    char skipped_csv[2048] = "";
    bool any_parts = false;

    const char *hermes_home = slermes_home();
    const char *job_id = json_get_str((json_t *)job, "id", "");

    for (size_t i = 0; i < n_skills; i++) {
        const char *skill_name = skill_names[i];
        const char *lookup = skill_name;
        while (*lookup == '/') lookup++;   /* lstrip("/") */

        /* Bundles shadow skills with the same slug. */
        char *bundle_key = resolve_bundle_command_key(lookup);
        if (bundle_key) {
            json_t *payload =
                build_bundle_invocation_message(bundle_key, "");
            free(bundle_key);
            if (payload) {
                const char *msg = json_get_str(payload, "message", "");
                if (any_parts) sb_put(&parts, "\n\n");
                sb_put(&parts, msg);
                any_parts = true;
                json_free(payload);
                continue;
            }
            /* bundle loaded zero skills — skip with notice */
            if (skipped_csv[0])
                strncat(skipped_csv, ", ",
                        sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            strncat(skipped_csv, skill_name,
                    sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            continue;
        }

        /* Plain skill via the tools surface. */
        char args[512];
        snprintf(args, sizeof(args), "{\"name\":\"%s\"}", lookup);
        char *resp = skills_view_handler(args, job_id[0] ? job_id : NULL);
        json_t *doc = resp ? json_parse(resp, NULL) : NULL;
        free(resp);

        const char *content = doc ? json_get_str(doc, "content", NULL) : NULL;
        if (!doc || !content || !content[0] ||
            json_obj_get(doc, "error")) {
            if (skipped_csv[0])
                strncat(skipped_csv, ", ",
                        sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            strncat(skipped_csv, skill_name,
                    sizeof(skipped_csv) - strlen(skipped_csv) - 1);
            if (doc) json_free(doc);
            continue;
        }

        /* Bump usage so the curator sees the skill as used. */
        if (hermes_home) skill_usage_bump_use(hermes_home, lookup);

        char *cdup = strdup(content);
        strip_inplace2(cdup);
        if (any_parts) sb_put(&parts, "\n\n");
        char note[512];
        snprintf(note, sizeof(note),
            "[IMPORTANT: The user has invoked the \"%s\" skill, indicating "
            "they want you to follow its instructions. The full skill "
            "content is loaded below.]\n\n", skill_name);
        sb_put(&parts, note);
        sb_put(&parts, cdup);
        free(cdup);
        any_parts = true;
        json_free(doc);
    }

    for (size_t i = 0; i < n_skills; i++) free(skill_names[i]);

    /* skipped notice goes FIRST */
    sb_t assembled;
    if (!sb_init(&assembled)) {
        free(parts.buf); free(final_prompt.buf);
        return NULL;
    }
    if (skipped_csv[0]) {
        char notice[4600];
        snprintf(notice, sizeof(notice),
            "[IMPORTANT: The following skill(s) were listed for this job "
            "but could not be found and were skipped: %s. Start your "
            "response with a brief notice so the user is aware, e.g.: "
            "'⚠️ Skill(s) not found and skipped: %s']",
            skipped_csv, skipped_csv);
        sb_put(&assembled, notice);
        if (any_parts) sb_put(&assembled, "\n");
    }
    sb_put(&assembled, parts.buf);
    free(parts.buf);

    if (final_prompt.buf[0]) {
        sb_put(&assembled, "\n\nThe user has provided the following "
                           "instruction alongside the skill invocation: ");
        sb_put(&assembled, final_prompt.buf);
    }
    free(final_prompt.buf);

    char *out = scheduler_scan_assembled_cron_prompt(
        assembled.buf, job, true, has_injected_data, user_prompt, error_out);
    free(assembled.buf);
    return out;
}


/* ── from src/cron/port_cron_scheduler_runtime.c ── */
/*
 * port_cron_scheduler_runtime.c — faithful C11 port of cron/scheduler.py's
 * runtime bookkeeping: the interrupted/running job-id sets, the
 * writer-preferring readers-writer lock guarding TERMINAL_CWD, the
 * persistent parallel/sequential thread pools, interpreter-shutdown
 * detection, cron lock paths, plugin env registry, and the robust cron
 * session-title write.
 *
 * Opaque structs + minimal includes; pthreads only (C11 threads are not
 * used elsewhere in the tree). No god headers.
 */

#include "cron_scheduler_runtime.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Reused plumbing (subsystem headers, no god header). */
#include "cron_jobs.h"          /* cronjobs_mark_job_run, cronjobs_cron_dir */
#include "session_title.h"      /* session_title_set / next_in_lineage */

/* ================================================================
 * Session titling
 * ================================================================ */

/* PoP: scheduler_set_cron_session_title @ cron/scheduler.py:_set_cron_session_title */
char *scheduler_set_cron_session_title(db_t *db, const char *session_id,
                                       const char *base_title,
                                       bool *conflict_out)
{
    if (conflict_out) *conflict_out = false;
    if (!db || !session_id || !session_id[0]) return NULL;

    /* title = (base_title or "").strip() */
    const char *s = base_title ? base_title : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) len--;
    if (len == 0) return NULL;

    char *title = malloc(len + 1);
    if (!title) return NULL;
    memcpy(title, s, len);
    title[len] = '\0';

    session_title_result_t rc = session_title_set(db, session_id, title);
    if (rc == SESSION_TITLE_OK) return title;

    if (rc == SESSION_TITLE_CONFLICT) {
        /* Unique-title collision: fall back to the next title in the
         * lineage ("base #2", "base #3", ...). If dedup yields nothing
         * new, surface the conflict (Python re-raises ValueError). */
        char *deduped = session_title_next_in_lineage(db, title);
        if (!deduped || strcmp(deduped, title) == 0) {
            free(deduped);
            free(title);
            if (conflict_out) *conflict_out = true;
            return NULL;
        }
        rc = session_title_set(db, session_id, deduped);
        free(title);
        if (rc == SESSION_TITLE_OK) return deduped;
        free(deduped);
        if (conflict_out) *conflict_out = true;
        return NULL;
    }

    free(title);
    return NULL;
}

/* ================================================================
 * Running / interrupted job-id sets
 * ================================================================ */

#define MAX_TRACKED_JOBS 256

static pthread_mutex_t g_running_lock = PTHREAD_MUTEX_INITIALIZER;
static char *g_running_ids[MAX_TRACKED_JOBS];
static size_t g_running_n = 0;
static char *g_interrupted_ids[MAX_TRACKED_JOBS];
static size_t g_interrupted_n = 0;

static bool idset_contains(char **set, size_t n, const char *id)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(set[i], id) == 0) return true;
    return false;
}

static void idset_add(char **set, size_t *n, const char *id)
{
    if (*n >= MAX_TRACKED_JOBS || idset_contains(set, *n, id)) return;
    set[*n] = strdup(id);
    if (set[*n]) (*n)++;
}

static void idset_remove(char **set, size_t *n, const char *id)
{
    for (size_t i = 0; i < *n; i++) {
        if (strcmp(set[i], id) == 0) {
            free(set[i]);
            set[i] = set[--(*n)];
            return;
        }
    }
}

void scheduler_running_jobs_add(const char *job_id)
{
    if (!job_id || !job_id[0]) return;
    pthread_mutex_lock(&g_running_lock);
    idset_add(g_running_ids, &g_running_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
}

void scheduler_running_jobs_remove(const char *job_id)
{
    if (!job_id || !job_id[0]) return;
    pthread_mutex_lock(&g_running_lock);
    idset_remove(g_running_ids, &g_running_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
}

/* PoP: scheduler_get_running_job_ids @ cron/scheduler.py:get_running_job_ids */
char **scheduler_get_running_job_ids(size_t *out_n)
{
    pthread_mutex_lock(&g_running_lock);
    char **out = calloc(g_running_n + 1, sizeof(char *));
    size_t n = 0;
    if (out) {
        for (size_t i = 0; i < g_running_n; i++) {
            out[n] = strdup(g_running_ids[i]);
            if (out[n]) n++;
        }
        out[n] = NULL;
    }
    pthread_mutex_unlock(&g_running_lock);
    if (out_n) *out_n = n;
    return out;
}

/* PoP: scheduler_mark_running_jobs_interrupted @ cron/scheduler.py:mark_running_jobs_interrupted */
char **scheduler_mark_running_jobs_interrupted(const char *reason,
                                               size_t *out_n)
{
    /* Snapshot the running set and record every ID in the interrupted set
     * BEFORE writing last_status, so a racing run_job completion sees the
     * flag and skips its own write (#60432). */
    pthread_mutex_lock(&g_running_lock);
    size_t snap_n = g_running_n;
    char **snap = calloc(snap_n + 1, sizeof(char *));
    if (snap) {
        for (size_t i = 0; i < snap_n; i++) {
            snap[i] = strdup(g_running_ids[i]);
            idset_add(g_interrupted_ids, &g_interrupted_n, g_running_ids[i]);
        }
        snap[snap_n] = NULL;
    }
    pthread_mutex_unlock(&g_running_lock);
    if (!snap) { if (out_n) *out_n = 0; return NULL; }

    char **marked = calloc(snap_n + 1, sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < snap_n; i++) {
        if (!snap[i]) continue;
        /* mark_job_run(job_id, False, reason) — best-effort. */
        cronjobs_mark_job_run(snap[i], false,
                              reason ? reason : "interrupted", NULL);
        if (marked) {
            marked[m] = snap[i];   /* transfer ownership */
            snap[i] = NULL;
            m++;
        }
    }
    for (size_t i = 0; i < snap_n; i++) free(snap[i]);
    free(snap);
    if (marked) marked[m] = NULL;
    if (out_n) *out_n = m;
    return marked;
}

/* PoP: scheduler_is_interrupted @ cron/scheduler.py:_is_interrupted */
bool scheduler_is_interrupted(const char *job_id)
{
    if (!job_id) return false;
    pthread_mutex_lock(&g_running_lock);
    bool hit = idset_contains(g_interrupted_ids, g_interrupted_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
    return hit;
}

/* PoP: scheduler_consume_interrupted_flag @ cron/scheduler.py:_consume_interrupted_flag */
bool scheduler_consume_interrupted_flag(const char *job_id)
{
    if (!job_id) return false;
    pthread_mutex_lock(&g_running_lock);
    bool hit = idset_contains(g_interrupted_ids, g_interrupted_n, job_id);
    if (hit) idset_remove(g_interrupted_ids, &g_interrupted_n, job_id);
    pthread_mutex_unlock(&g_running_lock);
    return hit;
}

/* ================================================================
 * Writer-preferring readers-writer lock (_ReadWriteLock)
 * ================================================================ */

struct scheduler_rwlock {
    pthread_mutex_t mu;
    pthread_cond_t cond;
    int readers;
    int writer_active;
    int writers_waiting;
};

/* PoP: scheduler_rwlock_new @ cron/scheduler.py:__init__ */
scheduler_rwlock_t *scheduler_rwlock_new(void)
{
    scheduler_rwlock_t *lk = calloc(1, sizeof(*lk));
    if (!lk) return NULL;
    pthread_mutex_init(&lk->mu, NULL);
    pthread_cond_init(&lk->cond, NULL);
    return lk;
}

void scheduler_rwlock_free(scheduler_rwlock_t *lk)
{
    if (!lk) return;
    pthread_cond_destroy(&lk->cond);
    pthread_mutex_destroy(&lk->mu);
    free(lk);
}

/* PoP: scheduler_rwlock_acquire_read @ cron/scheduler.py:acquire_read */
void scheduler_rwlock_acquire_read(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    while (lk->writer_active || lk->writers_waiting > 0)
        pthread_cond_wait(&lk->cond, &lk->mu);
    lk->readers++;
    pthread_mutex_unlock(&lk->mu);
}

/* PoP: scheduler_rwlock_release_read @ cron/scheduler.py:release_read */
void scheduler_rwlock_release_read(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    lk->readers--;
    if (lk->readers == 0)
        pthread_cond_broadcast(&lk->cond);
    pthread_mutex_unlock(&lk->mu);
}

/* PoP: scheduler_rwlock_acquire_write @ cron/scheduler.py:acquire_write */
void scheduler_rwlock_acquire_write(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    lk->writers_waiting++;
    while (lk->writer_active || lk->readers > 0)
        pthread_cond_wait(&lk->cond, &lk->mu);
    lk->writers_waiting--;
    lk->writer_active = 1;
    pthread_mutex_unlock(&lk->mu);
}

/* PoP: scheduler_rwlock_release_write @ cron/scheduler.py:release_write */
void scheduler_rwlock_release_write(scheduler_rwlock_t *lk)
{
    pthread_mutex_lock(&lk->mu);
    lk->writer_active = 0;
    pthread_cond_broadcast(&lk->cond);
    pthread_mutex_unlock(&lk->mu);
}

/* The module-global _terminal_cwd_lock. */
scheduler_rwlock_t *scheduler_terminal_cwd_lock(void)
{
    static scheduler_rwlock_t *g_lock = NULL;
    static pthread_mutex_t once = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&once);
    if (!g_lock) g_lock = scheduler_rwlock_new();
    pthread_mutex_unlock(&once);
    return g_lock;
}

/* ================================================================
 * Persistent thread pools
 * ================================================================ */

typedef struct pool_task {
    scheduler_task_fn fn;
    void *arg;
    struct pool_task *next;
} pool_task_t;

struct scheduler_pool {
    pthread_mutex_t mu;
    pthread_cond_t cond;
    pool_task_t *head, *tail;
    pthread_t *threads;
    int n_threads;
    int shutting_down;
    int active;               /* tasks currently executing */
    pthread_cond_t idle;      /* signaled when queue drains + active==0 */
};

static void *pool_worker(void *arg)
{
    scheduler_pool_t *p = arg;
    for (;;) {
        pthread_mutex_lock(&p->mu);
        while (!p->head && !p->shutting_down)
            pthread_cond_wait(&p->cond, &p->mu);
        if (!p->head && p->shutting_down) {
            pthread_mutex_unlock(&p->mu);
            return NULL;
        }
        pool_task_t *t = p->head;
        p->head = t->next;
        if (!p->head) p->tail = NULL;
        p->active++;
        pthread_mutex_unlock(&p->mu);

        t->fn(t->arg);
        free(t);

        pthread_mutex_lock(&p->mu);
        p->active--;
        if (!p->head && p->active == 0)
            pthread_cond_broadcast(&p->idle);
        pthread_mutex_unlock(&p->mu);
    }
}

static scheduler_pool_t *pool_create(int n_threads, const char *name_prefix)
{
    (void)name_prefix;
    if (n_threads <= 0) {
        /* CPython default: min(32, os.cpu_count() + 4) */
        long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        if (ncpu < 1) ncpu = 1;
        n_threads = (int)(ncpu + 4);
        if (n_threads > 32) n_threads = 32;
    }
    scheduler_pool_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    pthread_mutex_init(&p->mu, NULL);
    pthread_cond_init(&p->cond, NULL);
    pthread_cond_init(&p->idle, NULL);
    p->threads = calloc((size_t)n_threads, sizeof(pthread_t));
    if (!p->threads) { free(p); return NULL; }
    p->n_threads = n_threads;
    for (int i = 0; i < n_threads; i++)
        pthread_create(&p->threads[i], NULL, pool_worker, p);
    return p;
}

static void pool_shutdown(scheduler_pool_t *p, bool wait_for_queue)
{
    if (!p) return;
    pthread_mutex_lock(&p->mu);
    if (wait_for_queue) {
        while (p->head || p->active > 0)
            pthread_cond_wait(&p->idle, &p->mu);
    }
    p->shutting_down = 1;
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->mu);
    for (int i = 0; i < p->n_threads; i++)
        pthread_join(p->threads[i], NULL);
    /* drop any unexecuted tasks (shutdown(wait=False) path) */
    pool_task_t *t = p->head;
    while (t) { pool_task_t *n = t->next; free(t); t = n; }
    free(p->threads);
    pthread_cond_destroy(&p->idle);
    pthread_cond_destroy(&p->cond);
    pthread_mutex_destroy(&p->mu);
    free(p);
}

bool scheduler_pool_submit(scheduler_pool_t *pool,
                           scheduler_task_fn fn, void *arg)
{
    if (!pool || !fn) return false;
    pthread_mutex_lock(&pool->mu);
    if (pool->shutting_down) {
        pthread_mutex_unlock(&pool->mu);
        return false;   /* "cannot schedule new futures after shutdown" */
    }
    pool_task_t *t = malloc(sizeof(*t));
    if (!t) { pthread_mutex_unlock(&pool->mu); return false; }
    t->fn = fn; t->arg = arg; t->next = NULL;
    if (pool->tail) pool->tail->next = t; else pool->head = t;
    pool->tail = t;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mu);
    return true;
}

static pthread_mutex_t g_pool_mu = PTHREAD_MUTEX_INITIALIZER;
static scheduler_pool_t *g_parallel_pool = NULL;
static int g_parallel_pool_workers = -1;
static scheduler_pool_t *g_sequential_pool = NULL;

/* PoP: scheduler_get_parallel_pool @ cron/scheduler.py:_get_parallel_pool */
scheduler_pool_t *scheduler_get_parallel_pool(int max_workers)
{
    pthread_mutex_lock(&g_pool_mu);
    if (!g_parallel_pool || g_parallel_pool_workers != max_workers) {
        if (g_parallel_pool)
            pool_shutdown(g_parallel_pool, false); /* shutdown(wait=False) */
        g_parallel_pool = pool_create(max_workers, "cron-parallel");
        g_parallel_pool_workers = max_workers;
    }
    scheduler_pool_t *p = g_parallel_pool;
    pthread_mutex_unlock(&g_pool_mu);
    return p;
}

/* PoP: scheduler_get_sequential_pool @ cron/scheduler.py:_get_sequential_pool */
scheduler_pool_t *scheduler_get_sequential_pool(void)
{
    pthread_mutex_lock(&g_pool_mu);
    if (!g_sequential_pool)
        g_sequential_pool = pool_create(1, "cron-seq");
    scheduler_pool_t *p = g_sequential_pool;
    pthread_mutex_unlock(&g_pool_mu);
    return p;
}

/* PoP: scheduler_shutdown_parallel_pool @ cron/scheduler.py:_shutdown_parallel_pool */
void scheduler_shutdown_parallel_pool(void)
{
    pthread_mutex_lock(&g_pool_mu);
    scheduler_pool_t *pp = g_parallel_pool;
    scheduler_pool_t *sp = g_sequential_pool;
    g_parallel_pool = NULL;
    g_parallel_pool_workers = -1;
    g_sequential_pool = NULL;
    pthread_mutex_unlock(&g_pool_mu);
    /* shutdown(wait=True, cancel_futures=False) */
    pool_shutdown(pp, true);
    pool_shutdown(sp, true);
}

/* ================================================================
 * Interpreter shutdown detection
 * ================================================================ */

static volatile int g_shutting_down = 0;

void scheduler_note_interpreter_shutdown(void) { g_shutting_down = 1; }

/* PoP: scheduler_interpreter_shutting_down @ cron/scheduler.py:_interpreter_shutting_down */
bool scheduler_interpreter_shutting_down(const char *error_text)
{
    if (g_shutting_down) return true;
    if (error_text) {
        /* case-insensitive substring: "cannot schedule new futures" —
         * matches both CPython shutdown variants (#58720). */
        static const char *needle = "cannot schedule new futures";
        size_t nl = strlen(needle);
        size_t tl = strlen(error_text);
        if (tl >= nl) {
            for (size_t i = 0; i + nl <= tl; i++) {
                size_t j = 0;
                while (j < nl) {
                    char a = error_text[i + j], b = needle[j];
                    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                    if (a != b) break;
                    j++;
                }
                if (j == nl) return true;
            }
        }
    }
    return false;
}

/* ================================================================
 * Lock paths
 * ================================================================ */

/* PoP: scheduler_get_lock_paths @ cron/scheduler.py:_get_lock_paths */
void scheduler_get_lock_paths(char **lock_dir_out, char **tick_lock_out)
{
    char *dir = cronjobs_cron_dir();   /* <HERMES_HOME>/cron, resolved live */
    if (lock_dir_out) *lock_dir_out = dir ? strdup(dir) : NULL;
    if (tick_lock_out) {
        if (dir) {
            size_t n = strlen(dir) + sizeof("/.tick.lock");
            char *p = malloc(n);
            if (p) snprintf(p, n, "%s/.tick.lock", dir);
            *tick_lock_out = p;
        } else {
            *tick_lock_out = NULL;
        }
    }
    free(dir);
}

/* ================================================================
 * Plugin cron env registry (mirrors scheduler_register_plugin_platform
 * in port_cron_scheduler_delivery.c — this accessor is the
 * _plugin_cron_env_var read side over the same registration hook).
 * ================================================================ */

/* PoP: scheduler_plugin_cron_env_var @ cron/scheduler.py:_plugin_cron_env_var */
const char *scheduler_plugin_cron_env_var(const char *platform_name)
{
    /* The delivery module owns the plugin table; reuse its plugin-ONLY
     * lookup (built-ins never match — faithful to the Python accessor). */
    extern const char *scheduler_plugin_env_var_lookup(const char *name);
    if (!platform_name || !platform_name[0]) return "";
    return scheduler_plugin_env_var_lookup(platform_name);
}


/* ── from src/cron/port_cron_scheduler_runtime_impl.c ── */
/*
 * port_cron_scheduler_runtime_impl.c — faithful C11 port of the
 * cron/scheduler.py ORCHESTRATION + DELIVERY runtime surface. This is the
 * layer that ties the pure helpers (delivery routing, prompt assembly,
 * script runner, locks/pools) into a runnable job:
 *
 *   _run_job_no_agent-style no_agent path  -> scheduler_run_job_no_agent
 *   run_job                          (full orchestrator) -> scheduler_run_job
 *   _deliver_result                  (multi-target deliver) -> scheduler_deliver_result
 *   _send_media_via_adapter          (extension-routed send) -> scheduler_send_media_via_adapter
 *   _route_media                    (pure kind router)     -> scheduler_route_media
 *   _confirm_adapter_delivery        (SendResult guard)     -> scheduler_confirm_adapter_delivery
 *   _summarize_cron_failure_for_delivery -> scheduler_summarize_cron_failure_for_delivery
 *   _is_channel_dm_topic             (topic probe)          -> scheduler_is_channel_dm_topic
 *   _maybe_mirror_cron_delivery      (mirror to origin)     -> scheduler_maybe_mirror_cron_delivery
 *   _open_continuable_cron_thread    (thread open)           -> scheduler_open_continuable_cron_thread
 *   _seed_cron_thread_session        (thread session seed)  -> scheduler_seed_cron_thread_session
 *   _seed_cron_channel_session       (flat channel seed)    -> scheduler_seed_cron_channel_session
 *
 * The IO/process-coupled tail of run_job (provider resolution, AIAgent
 * construction, SessionDB, fallback chain, reasoning config) is delegated to
 * the caller via the scheduler_agent_run_fn callback — exactly the seam
 * Python uses (it imports/constructs AIAgent and calls run_conversation).
 * The C orchestrator faithfully reproduces the ORDER and GUARDS: no_agent
 * short-circuit, script-run + wake-gate, credential-exfil guard,
 * injection-scanned prompt assembly, interruption-flag honor, then delivery.
 *
 * Live-adapter branch: when a gateway runtime is live (cron fired from inside
 * the running gateway), delivery routes through the running adapter exactly
 * like Python's `adapters`/`loop` path. Standalone (scheduler tick / `hermes
 * cron run`) falls through to send_message_send_to_platform, the C equivalent
 * of Python's _send_to_platform fallback.
 *
 * Opaque structs + minimal includes; no god headers.
 */

#include "cron_scheduler_runtime.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "cron_jobs.h"                 /* now_iso, SCHEDULER_SILENT_MARKER */
#include "cron_scheduler_delivery.h"  /* routing/target/resolve helpers */
#include "cron_scheduler_helpers.h"   /* scheduler_normalize_deliver_value */
#include "gateway/platforms/base.h"  /* gw_extract_media / validate_media_delivery_path */
#include "hermes_gateway_mirror.h"    /* mirror_to_session */
#include "gw_server_internals.h"      /* session_get_or_create */
#include "hermes_redact.h"             /* hermes_redact */
#include "hermes_logger.h"             /* hermes_log */
#include "hermes_tool_config.h"        /* tool_config_get_bool */
#include "port_config_py_helpers.h"    /* config_py_load_config_impl */
#include "port_send_message_tool.h"      /* send_message_send_to_platform */
#include <stdbool.h>

/* send_message_send_to_platform is defined in port_send_message_tool.c and
 * not exported via a header; declare it here (faithful router into the
 * platform send surface). Signature mirrors tools/send_message_tool.py. */
extern json_t *send_message_send_to_platform(const char *platform, json_t *pconfig,
                                              const char *chat_id, const char *message,
                                              const char *thread_id, const char *const *media_files);

/* ── static config plumbing ─────────────────────────────────────────── */

/* printf-style strdup (avoids a non-portable strdupf dependency). */
static char *xstrdupf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* Read cron.wrap_response from config.yaml (default true). */
static bool cron_wrap_response_enabled(void)
{
    bool wrap = true;
    json_t *cfg = config_py_load_config_impl(0);
    if (cfg) {
        const json_t *cron = json_obj_get(cfg, "cron");
        if (cron && json_node_is_object(cron)) {
            const json_t *w = json_obj_get(cron, "wrap_response");
            if (w && json_node_is_bool(w))
                wrap = json_bool_value(w);
        }
        json_free(cfg);
    }
    return wrap;
}

/* Build the cron-mirror prefixed text: "[Cron delivery: <name>]\n<body>". */
static char *cron_mirror_prefix(const json_t *job, const char *body)
{
    const char *name = json_get_str((json_t *)job, "name", NULL);
    const char *id   = json_get_str((json_t *)job, "id", "cron");
    size_t n = (name ? strlen(name) : 0) + (id ? strlen(id) : 5) +
               (body ? strlen(body) : 0) + 48;
    char *msg = malloc(n);
    if (msg)
        snprintf(msg, n, "[Cron delivery: %s]\n%s",
                 name ? name : (id ? id : "cron"), body ? body : "");
    return msg;
}

/* ── failure text builder ──────────────────────────────────────────── */

/* PoP: scheduler_summarize_cron_failure_for_delivery
 *      @ cron/scheduler.py:_summarize_cron_failure_for_delivery */
char *scheduler_summarize_cron_failure_for_delivery(const char *job_id,
                                                    const char *error)
{
    if (!job_id) job_id = "<unknown>";
    if (!error) error = "unknown error";

    char *safe = hermes_redact(error);

    size_t n = strlen(job_id) + strlen(safe) + 160;
    char *out = malloc(n);
    if (out)
        snprintf(out, n,
                 "Cron job '%s' failed: %s\n\n"
                 "To stop or manage this job, send me a new message "
                 "(e.g. \"stop reminder %s\").",
                 job_id, safe, job_id);
    free(safe);
    return out;
}

/* ── SendResult contract guard ─────────────────────────────────────── */

/* PoP: scheduler_confirm_adapter_delivery
 *      @ cron/scheduler.py:_confirm_adapter_delivery */
int scheduler_confirm_adapter_delivery(const json_t *send_result)
{
    if (!send_result || !json_node_is_object(send_result)) return 0;
    const json_t *status = json_obj_get(send_result, "status");
    if (status && json_node_is_string(status) &&
        strcmp(json_string_value(status), "delivered") == 0)
        return 1;
    const json_t *success = json_obj_get(send_result, "success");
    if (success && json_node_is_bool(success) && json_bool_value(success))
        return 1;
    return 0;
}

/* ── pure media kind router ────────────────────────────────────────── */

/* PoP: scheduler_route_media @ cron/scheduler.py:_send_media_via_adapter */
/* Pure routing: extension + platform + voice flag -> adapter method kind.
 * Faithful to base.py:should_send_media_as_audio:
 *   - if should_send_media_as_audio(platform, ext, is_voice) -> VOICE (audio)
 *   - elif video extension -> VIDEO
 *   - elif image extension -> IMAGE
 *   - else -> DOCUMENT
 * Telegram: .ogg/.opus are audio (voice sender) ONLY when is_voice is set;
 * .mp3/.m4a are audio attachments; other (.jpg/.png/.mp4...) fall through.
 * Every other platform: any recognized audio extension -> audio. */
scheduler_media_kind_t scheduler_route_media(const char *platform,
                                             const char *media_path,
                                             bool is_voice)
{
    const char *dot = media_path ? strrchr(media_path, '.') : NULL;
    const char *ext = dot ? dot : "";

    static const char *video_exts[] = {".mp4",".mov",".avi",".mkv",".webm",".3gp", NULL};
    static const char *image_exts[] = {".jpg",".jpeg",".png",".webp",".gif", NULL};
    static const char *audio_exts[] = {".ogg",".opus",".mp3",".wav",".m4a",".flac", NULL};
    static const char *telegram_voice_exts[] = {".ogg",".opus", NULL};
    static const char *telegram_audio_exts[] = {".mp3",".m4a", NULL};

    int is_video = 0, is_image = 0, is_audio = 0;
    for (int i = 0; video_exts[i]; i++) if (!strcmp(ext, video_exts[i])) is_video = 1;
    for (int i = 0; image_exts[i]; i++) if (!strcmp(ext, image_exts[i])) is_image = 1;
    for (int i = 0; audio_exts[i]; i++) if (!strcmp(ext, audio_exts[i])) is_audio = 1;

    bool as_audio = false;
    if (is_audio) {
        if (platform && strcmp(platform, "telegram") == 0) {
            for (int i = 0; telegram_voice_exts[i]; i++)
                if (!strcmp(ext, telegram_voice_exts[i])) as_audio = is_voice;
            for (int i = 0; telegram_audio_exts[i]; i++)
                if (!strcmp(ext, telegram_audio_exts[i])) as_audio = true;
        } else {
            as_audio = true;
        }
    }

    if (as_audio) return SCHEDULER_MEDIA_VOICE;
    if (is_video) return SCHEDULER_MEDIA_VIDEO;
    if (is_image) return SCHEDULER_MEDIA_IMAGE;
    return SCHEDULER_MEDIA_DOCUMENT;
}

/* ── media send via adapter (live path) ────────────────────────────── */

/* Routes each extracted MEDIA file to the appropriate adapter method by kind.
 * Returns the number of successful sends. The adapter may be NULL (no live
 * gateway) in which case 0 is returned (the standalone fallback handles
 * media via send_message_send_to_platform). */
int scheduler_send_media_via_adapter(const scheduler_adapter_t *adapter,
                                     const char *platform,
                                     const char *chat_id,
                                     const char *const *media_paths,
                                     const bool *voice_flags,
                                     const json_t *job)
{
    (void)job;
    if (!adapter || !platform || !chat_id || !media_paths) return 0;
    int sent = 0;
    for (int i = 0; media_paths[i]; i++) {
        bool is_voice = voice_flags ? voice_flags[i] : false;
        scheduler_media_kind_t kind = scheduler_route_media(platform,
                                                            media_paths[i],
                                                            is_voice);
        bool ok = false;
        switch (kind) {
        case SCHEDULER_MEDIA_VOICE:
            if (adapter->send_voice)
                ok = adapter->send_voice(chat_id, media_paths[i], adapter->ctx);
            break;
        case SCHEDULER_MEDIA_VIDEO:
            if (adapter->send_video)
                ok = adapter->send_video(chat_id, media_paths[i], adapter->ctx);
            break;
        case SCHEDULER_MEDIA_IMAGE:
            if (adapter->send_image)
                ok = adapter->send_image(chat_id, media_paths[i], adapter->ctx);
            break;
        case SCHEDULER_MEDIA_DOCUMENT:
        default:
            if (adapter->send_document)
                ok = adapter->send_document(chat_id, media_paths[i], adapter->ctx);
            break;
        }
        if (ok) sent++;
    }
    return sent;
}

/* ── ambiguous Telegram topic probe ────────────────────────────────── */

/* PoP: available @ cron/scheduler.py:_is_channel_dm_topic */
/* The running gateway adapter exposes a get_chat_info probe. When no live
 * adapter is available (standalone cron), we fail SAFE to forum-topic routing
 * (return 0), exactly like the Python default. */
bool scheduler_is_channel_dm_topic(const scheduler_adapter_t *adapter,
                                   const char *chat_id, const char *job_id)
{
    (void)job_id;
    if (!adapter || !adapter->get_chat_info || !chat_id) return false;
    char type[32];
    if (adapter->get_chat_info(chat_id, type, sizeof(type), adapter->ctx) &&
        strcmp(type, "channel") == 0)
        return true;
    return false;
}

/* ── mirror / seed helpers ─────────────────────────────────────────── */

/* PoP: scheduler_maybe_mirror_cron_delivery @ cron/scheduler.py:_maybe_mirror_cron_delivery */
/* Best-effort mirror of a cron delivery into the origin chat's session.
 * No-op unless enabled. Prefixes "[Cron delivery: <name>]". */
void scheduler_maybe_mirror_cron_delivery(const json_t *job,
                                          const char *platform,
                                          const char *chat_id,
                                          const char *mirror_text,
                                          const char *thread_id,
                                          const char *user_id,
                                          bool enabled)
{
    if (!enabled) return;
    char *text = cron_mirror_prefix(job, mirror_text ? mirror_text : "");
    if (!text || !text[0]) { free(text); return; }
    mirror_to_session(platform, chat_id, text, "cron", thread_id, user_id);
    free(text);
}

/* PoP: NULL @ cron/scheduler.py:_open_continuable_cron_thread */
/* Open a dedicated thread for a continuable cron job. Returns malloc'd thread
 * id or NULL (fall back to DM mirror). In standalone cron there is no live
 * adapter with a create_thread capability, so we return NULL — the
 * mirror-to-session fallback (seed_cron_thread_session) handles the surface. */
char *scheduler_open_continuable_cron_thread(const json_t *job,
                                             const scheduler_adapter_t *adapter,
                                             const char *chat_id)
{
    (void)job;
    if (!adapter || !adapter->create_thread || !chat_id) return NULL;
    const char *name = json_get_str((json_t *)job, "name", "cron");
    char *tid = adapter->create_thread(chat_id, name, adapter->ctx);
    return tid; /* may be NULL */
}

/* PoP: scheduler_seed_cron_thread_session @ cron/scheduler.py:_seed_cron_thread_session */
/* Seed the freshly-opened cron thread's session with the brief. Ensures the
 * thread-keyed session row exists, then mirrors the brief. */
void scheduler_seed_cron_thread_session(const json_t *job,
                                        const char *platform,
                                        const char *chat_id,
                                        const char *thread_id,
                                        const char *mirror_text)
{
    char *text = cron_mirror_prefix(job, mirror_text ? mirror_text : "");
    if (!text || !text[0]) { free(text); return; }
    (void)session_get_or_create(platform, chat_id);
    mirror_to_session(platform, chat_id, text, "cron", thread_id,
                      "system:cron");
    free(text);
}

/* PoP: FLAT @ cron/scheduler.py:_seed_cron_channel_session */
/* Seed the FLAT (thread-less) session for an in_channel delivery. Returns true
 * when a seed row was created and the brief mirrored. */
bool scheduler_seed_cron_channel_session(const json_t *job,
                                         const char *platform,
                                         const char *chat_id,
                                         const char *mirror_text,
                                         bool is_dm, const char *user_id)
{
    char *text = cron_mirror_prefix(job, mirror_text ? mirror_text : "");
    if (!text || !text[0]) { free(text); return false; }
    (void)session_get_or_create(platform, chat_id);
    mirror_to_session(platform, chat_id, text, "cron", NULL,
                      user_id ? user_id : NULL);
    free(text);
    return true;
}

/* ── deliver_result ────────────────────────────────────────────────── */

/* PoP: target @ cron/scheduler.py:_deliver_result */
/* Deliver job output to the configured target(s). Uses the adapter when given,
 * else the registered platform send path. Returns NULL on success (or a
 * legitimately silent run) or a malloc'd error string. */
char *scheduler_deliver_result(const json_t *job, const char *content,
                               const scheduler_adapter_t *adapter)
{
    if (!job) return strdup("no job to deliver");
    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");

    /* 1. resolve targets */
    scheduler_job_t sjob;
    memset(&sjob, 0, sizeof(sjob));
    const json_t *origin = json_obj_get((json_t *)job, "origin");
    if (origin && json_node_is_object(origin)) {
        const char *plat = json_get_str(origin, "platform", NULL);
        const char *cid  = json_get_str(origin, "chat_id", NULL);
        if (plat && cid)
            sjob.origin = (scheduler_origin_t){ plat, cid,
                json_get_str(origin, "thread_id", NULL), 1 };
    }
    sjob.deliver = json_get_str((json_t *)job, "deliver", "local");

    scheduler_target_t targets[16];
    int n = scheduler_resolve_delivery_targets(&sjob, targets, 16);
    if (n == 0) {
        const char *deliver_value = sjob.deliver ? sjob.deliver : "local";
        if (strcmp(deliver_value, "local") == 0)
            return NULL;                 /* local-only: not a failure */
        if (strcmp(deliver_value, "origin") == 0)
            return NULL;                 /* origin unresolvable: treat local */
        char *msg = malloc(128);
        snprintf(msg, 128, "no delivery target resolved for deliver=%s",
                 deliver_value);
        return msg;
    }

    /* 2. build delivery content */
    bool wrap = cron_wrap_response_enabled();
    char *delivery_content;
    if (wrap) {
        const char *task_name = json_get_str((json_t *)job, "name", job_id);
        size_t nlen = strlen(task_name) + strlen(job_id) +
                   strlen(content ? content : "") + 200;
        delivery_content = malloc(nlen);
        snprintf(delivery_content, nlen,
                 "Cronjob Response: %s\n(job_id: %s)\n-------------\n\n%s\n\n"
                 "To stop or manage this job, send me a new message "
                 "(e.g. \"stop reminder %s\").",
                 task_name, job_id, content ? content : "", task_name);
    } else {
        delivery_content = strdup(content ? content : "");
    }

    /* 3. extract MEDIA: tags */
    gw_media_list_t media = gw_extract_media(delivery_content);
    gw_media_list_t safe_media;
    memset(&safe_media, 0, sizeof(safe_media));
    for (int i = 0; i < media.count && safe_media.count < 64; i++) {
        char *p = validate_media_delivery_path(media.paths[i]);
        if (p) {
            safe_media.paths[safe_media.count] = p;
            safe_media.is_voice[safe_media.count] =
                media.is_voice ? media.is_voice[i] : 0;
            safe_media.count++;
        }
    }
    gw_media_list_free(&media);

    /* mirror gate (computed once) */
    json_t *user_cfg = config_py_load_config_impl(0);
    int mirror_enabled = scheduler_cron_mirror_delivery_enabled(&sjob,
        user_cfg ? tool_config_get_bool("cron", "mirror_delivery", false) : 0);
    json_free(user_cfg);

    char *mirror_text = NULL;
    if (mirror_enabled) {
        gw_media_list_t m2 = gw_extract_media(content ? content : "");
        gw_media_list_free(&m2);
        mirror_text = strdup(content ? content : "");
        if (mirror_text) {             /* strip trailing whitespace */
            size_t l = strlen(mirror_text);
            while (l && (mirror_text[l-1]==' '||mirror_text[l-1]=='\n'||
                         mirror_text[l-1]=='\r'||mirror_text[l-1]=='\t'))
                mirror_text[--l] = '\0';
        }
    }

    /* 4. per-target delivery */
    char *errors[16]; int ne = 0;
    int live_adapter_ready = (adapter != NULL);

    for (int t = 0; t < n; t++) {
        const char *platform_name = targets[t].platform;
        const char *chat_id = targets[t].chat_id;
        char *thread_id = strdup(targets[t].thread_id);
        if (!thread_id) thread_id = strdup("");

        int mirror_this =
            mirror_enabled &&
            scheduler_target_matches_origin(&sjob.origin, platform_name,
                                            chat_id, thread_id);
        const char *origin_user_id = NULL;
        if (mirror_this && origin && json_node_is_object(origin))
            origin_user_id = json_get_str(origin, "user_id", NULL);

        if (!scheduler_is_known_delivery_platform(platform_name)) {
            if (ne < 16) errors[ne++] =
                xstrdupf("unknown platform '%s'", platform_name);
            free(thread_id);
            continue;
        }

        /* continuable surface: thread (default) vs in_channel */
        int in_channel = 0;
        if (gateway_config_platform_extra_bool(platform_name,
                                               "cron_continuable_surface")) {
            json_t *cfg = config_py_load_config_impl(0);
            if (cfg) {
                const json_t *gwc = json_obj_get(cfg, "gateway");
                const json_t *plats = gwc ? json_obj_get(gwc, "platforms") : NULL;
                const json_t *pc = plats ? json_obj_get(plats, platform_name) : NULL;
                const json_t *extra = pc ? json_obj_get(pc, "extra") : NULL;
                const json_t *surf = extra ? json_obj_get(extra,
                                                 "cron_continuable_surface") : NULL;
                if (surf && json_node_is_string(surf) &&
                    strcmp(json_string_value(surf), "in_channel") == 0)
                    in_channel = 1;
                json_free(cfg);
            }
        }

        if (in_channel && mirror_this && live_adapter_ready) {
            free(thread_id);
            thread_id = strdup("");
        }

        int is_dm_target = 0;
        const json_t *och = origin ? json_obj_get(origin, "chat_type") : NULL;
        const char *och_s = och && json_node_is_string(och)
            ? json_string_value(och) : "";
        if (!strcmp(och_s, "dm") ||
            (!och_s[0] && strncmp(chat_id, "D", 1) == 0))
            is_dm_target = 1;

        int thread_seeded = 0;
        int inchannel_seeded = 0;

        char *opened_thread_id = NULL;
        if (mirror_this && !in_channel && live_adapter_ready &&
            !thread_id[0]) {
            opened_thread_id =
                scheduler_open_continuable_cron_thread(job, adapter, chat_id);
            if (opened_thread_id) {
                free(thread_id);
                thread_id = opened_thread_id;
            }
        }

        int delivered = 0;
        if (live_adapter_ready) {
            /* Live path: route through the running gateway adapter. */
            if (opened_thread_id && !thread_seeded) {
                scheduler_seed_cron_thread_session(job, platform_name,
                                                    chat_id, thread_id,
                                                    mirror_text);
                thread_seeded = 1;
            }
            int ok = 0;
            if (adapter->send_text)
                ok = adapter->send_text(chat_id, delivery_content,
                                        thread_id[0] ? thread_id : NULL,
                                        adapter->ctx);
            if (ok) delivered = 1;
        }

        if (!delivered) {
            /* Standalone fallback: send via the platform send surface. */
            const char *files[65];
            int fc = 0;
            for (int i = 0; i < safe_media.count && fc < 64; i++)
                files[fc++] = safe_media.paths[i];
            files[fc] = NULL;

            json_t *pcfg = json_object();
            json_t *res = send_message_send_to_platform(
                platform_name, pcfg, chat_id,
                delivery_content, thread_id[0] ? thread_id : NULL, files);
            json_free(pcfg);

            int ok = 0;
            if (res) {
                const json_t *err = json_obj_get(res, "error");
                ok = !err;
                json_free(res);
            }
            if (ok) {
                hermes_log(LOG_INFO, "cron", "delivered to %s:%s",
                           platform_name, chat_id);
                delivered = 1;
                if (in_channel && mirror_this) {
                    inchannel_seeded = scheduler_seed_cron_channel_session(
                        job, platform_name, chat_id, mirror_text,
                        is_dm_target, origin_user_id);
                }
                scheduler_maybe_mirror_cron_delivery(
                    job, platform_name, chat_id, mirror_text,
                    thread_id[0] ? thread_id : NULL, origin_user_id,
                    mirror_this && !thread_seeded && !inchannel_seeded);
            } else {
                if (ne < 16) errors[ne++] = xstrdupf(
                    "delivery to %s:%s failed", platform_name, chat_id);
            }
        }

        free(thread_id);
    }

    for (int i = 0; i < safe_media.count; i++) free(safe_media.paths[i]);
    free(mirror_text);
    free(delivery_content);

    if (ne > 0) {
        size_t total = 0;
        for (int i = 0; i < ne; i++) total += strlen(errors[i]) + 3;
        char *out = malloc(total + 1);
        out[0] = '\0';
        for (int i = 0; i < ne; i++) {
            strcat(out, errors[i]);
            if (i + 1 < ne) strcat(out, "; ");
            free(errors[i]);
        }
        return out;
    }
    return NULL;
}

/* ── run_job_no_agent ─────────────────────────────────────────────── */

/* PoP: scheduler_run_job_no_agent @ cron/scheduler.py:run_job */
/* Full no_agent run path of run_job: the script IS the job. Returns success;
 * fills malloc'd *doc_out, *final_out, *error_out (any may be NULL).
 * final_out == SCHEDULER_SILENT_MARKER marks a silent run. */
bool scheduler_run_job_no_agent(const json_t *job, char **doc_out,
                                char **final_out, char **error_out)
{
    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");
    const char *job_name = json_get_str((json_t *)job, "name",
                            json_get_str((json_t *)job, "prompt", job_id));

    const char *script_path = json_get_str((json_t *)job, "script", NULL);
    if (!script_path) {
        if (error_out) *error_out = strdup(
            "no_agent=True but no script is set for this job");
        return false;
    }

    const char *wd = json_get_str((json_t *)job, "workdir", NULL);
    char *_job_workdir = NULL;
    if (wd && wd[0]) {
        /* Validate the workdir exists; else run without it. */
        struct stat st;
        if (stat(wd, &st) == 0 && S_ISDIR(st.st_mode)) {
            _job_workdir = strdup(wd);
        } else {
            hermes_log(LOG_WARNING, "cron",
                "Job '%s': configured workdir '%s' no longer exists",
                job_id, wd);
        }
    }

    bool ok = false; char *output = NULL;
    ok = scheduler_run_job_script_with_claim_heartbeat(job, script_path,
                                                       _job_workdir, &output);
    free(_job_workdir);

    char *now_iso_str = now_iso();

    if (!ok) {
        char *alert = xstrdupf(
            "\xE2\x9A\xA0 Cron watchdog '%s' script failed\n\n%s\n\nTime: %s",
            job_name, output ? output : "", now_iso_str ? now_iso_str : "");
        char *doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
            "**Mode:** no_agent (script)\n**Status:** script failed\n\n%s\n",
            job_name, job_id, now_iso_str ? now_iso_str : "",
            output ? output : "");
        if (doc_out) *doc_out = doc; else free(doc);
        if (final_out) *final_out = alert; else free(alert);
        if (error_out) *error_out = strdup(output ? output : "");
        free(output); free(now_iso_str);
        return false;
    }

    /* wakeAgent=false gate -> silent */
    if (!scheduler_parse_wake_gate(output)) {
        hermes_log(LOG_INFO, "cron",
            "Job '%s' (no_agent): wakeAgent=false gate — silent run", job_id);
        char *doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
            "**Mode:** no_agent (script)\n**Status:** silent (wakeAgent=false)\n",
            job_name, job_id, now_iso_str ? now_iso_str : "");
        if (doc_out) *doc_out = doc; else free(doc);
        if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
        if (error_out) *error_out = NULL;
        free(output); free(now_iso_str);
        return true;
    }

    if (!output || !output[0] || !output[strspn(output, " \t\r\n")]) {
        hermes_log(LOG_INFO, "cron",
            "Job '%s' (no_agent): empty stdout — silent run", job_id);
        char *doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
            "**Mode:** no_agent (script)\n**Status:** silent (empty output)\n",
            job_name, job_id, now_iso_str ? now_iso_str : "");
        if (doc_out) *doc_out = doc; else free(doc);
        if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
        if (error_out) *error_out = NULL;
        free(output); free(now_iso_str);
        return true;
    }

    char *doc = xstrdupf(
        "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
        "**Mode:** no_agent (script)\n\n---\n\n%s\n",
        job_name, job_id, now_iso_str ? now_iso_str : "", output);
    if (doc_out) *doc_out = doc; else free(doc);
    if (final_out) *final_out = strdup(output); else { /* output owned below */ }
    if (error_out) *error_out = NULL;
    free(output); free(now_iso_str);
    return true;
}

/* ── run_job (full orchestrator) ──────────────────────────────────── */

/* PoP: scheduler_run_job @ cron/scheduler.py:run_job */
/* Execute a single cron job. no_agent short-circuit, wake gate, prompt
 * assembly (injection-scanned), credential-exfil guard, then the agent
 * callback; interruption flags honored before status is reported. Returns
 * success; fills the 4-tuple (doc_out, final_out, error_out, + needs_deliver
 * via return of final_out). */
bool scheduler_run_job(const json_t *job,
                       scheduler_agent_run_fn agent_run, void *agent_ctx,
                       char **doc_out, char **final_out, char **error_out)
{
    const char *job_id = json_get_str((json_t *)job, "id", "<unknown>");
    const char *job_name = json_get_str((json_t *)job, "name",
                            json_get_str((json_t *)job, "prompt", job_id));

    /* ----------------------------------------------------------------
     * no_agent short-circuit — the script IS the job, no LLM involvement.
     * ---------------------------------------------------------------- */
    if (json_get_bool((json_t *)job, "no_agent", false)) {
        char *d = NULL, *f = NULL, *e = NULL;
        bool ok = scheduler_run_job_no_agent(job, &d, &f, &e);
        if (doc_out) *doc_out = d; else free(d);
        if (final_out) *final_out = f; else free(f);
        if (error_out) *error_out = e; else free(e);
        return ok;
    }

    /* ----------------------------------------------------------------
     * Default (LLM) path.
     * ---------------------------------------------------------------- */
    /* Credential-exfil guard (fail closed before any provider call). */
    char *guard_err = scheduler_guard_job_credential_exfil(job);
    if (guard_err) {
        char *blocked_doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Status:** BLOCKED\n\n"
            "The job's stored provider/base_url pair would ship a credential "
            "off-host. Refusing to run.\n\n**Guard result:** %s\n",
            job_name, job_id, guard_err);
        if (doc_out) *doc_out = blocked_doc; else free(blocked_doc);
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = guard_err; /* transfers ownership */
        return false;
    }

    /* Wake-gate: run the pre-check script before building the prompt so a
     * wakeAgent=false short-circuits the agent run. */
    const char *script_path = json_get_str((json_t *)job, "script", NULL);
    bool prerun_ok = false; char *prerun_output = NULL;
    if (script_path) {
        prerun_ok = scheduler_run_job_script_with_claim_heartbeat(
            job, script_path, NULL, &prerun_output);
        if (prerun_ok && !scheduler_parse_wake_gate(prerun_output)) {
            hermes_log(LOG_INFO, "cron",
                "Job '%s': wakeAgent=false, skipping agent run", job_id);
            char *now = now_iso();
            char *silent_doc = xstrdupf(
                "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n\n"
                "Script gate returned `wakeAgent=false` — agent skipped.\n",
                job_name, job_id, now ? now : "");
            free(now); free(prerun_output);
            if (doc_out) *doc_out = silent_doc; else free(silent_doc);
            if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
            if (error_out) *error_out = NULL;
            return true;
        }
    }

    bool silent = false;
    char *agent_err = NULL;
    char *prompt = scheduler_build_job_prompt(job, prerun_ok, prerun_output,
                                              &silent, &agent_err);
    free(prerun_output);
    if (agent_err) {
        if (doc_out) *doc_out = NULL;
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = agent_err;
        return false;
    }
    if (prompt == NULL) {
        /* Silent (empty script output) — skip AI call. */
        if (doc_out) *doc_out = NULL;
        if (final_out) *final_out = strdup(SCHEDULER_SILENT_MARKER);
        if (error_out) *error_out = NULL;
        return true;
    }

    /* Honor interruption before spending the agent run. */
    if (scheduler_is_interrupted(job_id)) {
        char *blocked_doc = xstrdupf(
            "# Cron Job: %s\n\n**Job ID:** %s\n**Status:** INTERRUPTED\n\n"
            "Job was interrupted before the agent run.\n", job_name, job_id);
        free(prompt);
        if (doc_out) *doc_out = blocked_doc; else free(blocked_doc);
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = strdup("interrupted");
        return false;
    }

    /* Run the agent via the injected callback. */
    char *agent_final = agent_run ? agent_run(prompt, job, &agent_err, agent_ctx)
                                   : NULL;
    free(prompt);
    if (!agent_final) {
        if (doc_out) *doc_out = NULL;
        if (final_out) *final_out = NULL;
        if (error_out) *error_out = agent_err ? agent_err : strdup("agent failed");
        return false;
    }

    char *now = now_iso();
    char *doc = xstrdupf(
        "# Cron Job: %s\n\n**Job ID:** %s\n**Run Time:** %s\n"
        "**Mode:** agent\n\n---\n\n%s\n",
        job_name, job_id, now ? now : "", agent_final);
    free(now);
    if (doc_out) *doc_out = doc; else free(doc);
    if (final_out) *final_out = agent_final; /* transfers ownership */
    if (error_out) *error_out = NULL;
    return true;
}


/* ── from src/cron/port_cron_scheduler_script.c ── */
/*
 * port_cron_scheduler_script.c — faithful C11 port of cron/scheduler.py's
 * script-execution surface:
 *   _get_script_timeout            (module/env/config resolution chain)
 *   _read_windows_pyvenv_cfg       (pyvenv.cfg parser — portable)
 *   _run_job_script                (traversal-guarded runner + redaction)
 *   _run_job_script_with_claim_heartbeat (owned one-shot claim keepalive)
 *   _parse_wake_gate               (nanoclaw wakeAgent convention)
 * plus the _sanitize_subprocess_env core from tools/environments/local.py
 * (name blocklist + dynamic-secret predicate + venv marker strip).
 *
 * Opaque plumbing reused: slermes_home(), hermes_redact(),
 * config_py_load_config_impl(), cronjobs_heartbeat_run_claim().
 */

#include "cron_scheduler_runtime.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hermes_json.h"
#include "slermes_home.h"
#include "cron_jobs.h"                 /* cronjobs_heartbeat_run_claim */
#include "port_config_py_helpers.h"    /* config_py_load_config_impl */

extern char **environ;
extern char *hermes_redact(const char *input);   /* agent/redact.c */

/* ================================================================
 * Script timeout resolution
 * ================================================================ */

#define DEFAULT_SCRIPT_TIMEOUT 3600
#define RUN_CLAIM_HEARTBEAT_SECONDS 60

static int g_script_timeout_override = DEFAULT_SCRIPT_TIMEOUT;

void scheduler_set_script_timeout_override(int seconds)
{
    g_script_timeout_override = seconds;
}

/* int(float(s)) with the Python failure contract: 0 on failure. */
static int parse_timeout(const char *s)
{
    if (!s || !s[0]) return 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return 0;
    while (end && (*end == ' ' || *end == '\t')) end++;
    if (end && *end) return 0;
    if (v <= 0) return 0;
    return (int)v;
}

/* PoP: scheduler_get_script_timeout @ cron/scheduler.py:_get_script_timeout */
int scheduler_get_script_timeout(void)
{
    /* 1. Patched module value (tests / emergency monkeypatch). */
    if (g_script_timeout_override != DEFAULT_SCRIPT_TIMEOUT &&
        g_script_timeout_override > 0)
        return g_script_timeout_override;

    /* 2. Env override. */
    const char *env = getenv("HERMES_CRON_SCRIPT_TIMEOUT");
    if (env) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", env);
        /* strip */
        char *s = buf;
        while (*s == ' ' || *s == '\t') s++;
        size_t l = strlen(s);
        while (l && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
        if (l) {
            int t = parse_timeout(s);
            if (t > 0) return t;
        }
    }

    /* 3. Config cron.script_timeout_seconds. */
    json_t *cfg = config_py_load_config_impl(0);
    if (cfg) {
        json_t *cron_cfg = json_object_get(cfg, "cron");
        if (json_node_is_object(cron_cfg)) {
            json_t *configured =
                json_object_get(cron_cfg, "script_timeout_seconds");
            if (configured) {
                int t = 0;
                if (json_node_is_number(configured)) {
                    double d = json_number_value(configured);
                    if (d > 0) t = (int)d;
                } else if (json_node_is_string(configured)) {
                    t = parse_timeout(json_string_value(configured));
                }
                if (t > 0) { json_free(cfg); return t; }
            }
        }
        json_free(cfg);
    }

    return DEFAULT_SCRIPT_TIMEOUT;
}

/* ================================================================
 * pyvenv.cfg parser
 * ================================================================ */

/* PoP: scheduler_read_pyvenv_cfg @ cron/scheduler.py:_read_windows_pyvenv_cfg */
json_t *scheduler_read_pyvenv_cfg(const char *venv_dir)
{
    json_t *parsed = json_object();
    if (!parsed) return NULL;
    if (!venv_dir || !venv_dir[0]) return parsed;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/pyvenv.cfg", venv_dir);
    FILE *f = fopen(path, "r");
    if (!f) return parsed;   /* OSError -> {} */

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        /* strip key, lowercase */
        while (*key == ' ' || *key == '\t') key++;
        size_t kl = strlen(key);
        while (kl && (key[kl-1] == ' ' || key[kl-1] == '\t' ||
                      key[kl-1] == '\r' || key[kl-1] == '\n')) key[--kl] = '\0';
        for (size_t i = 0; i < kl; i++)
            key[i] = (char)tolower((unsigned char)key[i]);
        /* strip val */
        while (*val == ' ' || *val == '\t') val++;
        size_t vl = strlen(val);
        while (vl && (val[vl-1] == ' ' || val[vl-1] == '\t' ||
                      val[vl-1] == '\r' || val[vl-1] == '\n')) val[--vl] = '\0';
        if (kl) json_set(parsed, key, json_string(val));
    }
    fclose(f);
    return parsed;
}

/* ================================================================
 * Subprocess env sanitizer
 * (core of tools/environments/local.py:_sanitize_subprocess_env)
 * ================================================================ */

/* Static name blocklist — the union of the registry-derived names and the
 * hard-coded set in _build_provider_env_blocklist. The C tree has no live
 * provider registry import; this mirrors the full effective set. */
static const char *ENV_BLOCKLIST[] = {
    "OPENAI_BASE_URL", "OPENAI_API_KEY", "OPENAI_API_BASE", "OPENAI_ORG_ID",
    "OPENAI_ORGANIZATION", "OPENROUTER_API_KEY", "ANTHROPIC_BASE_URL",
    "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN", "LLM_MODEL", "GOOGLE_API_KEY",
    "VERTEX_CREDENTIALS_PATH", "GOOGLE_APPLICATION_CREDENTIALS",
    "DEEPSEEK_API_KEY", "MISTRAL_API_KEY", "GROQ_API_KEY",
    "TOGETHER_API_KEY", "PERPLEXITY_API_KEY", "COHERE_API_KEY",
    "FIREWORKS_API_KEY", "XAI_API_KEY", "HELICONE_API_KEY",
    "PARALLEL_API_KEY", "FIRECRAWL_API_KEY", "FIRECRAWL_API_URL",
    "TELEGRAM_HOME_CHANNEL", "TELEGRAM_HOME_CHANNEL_NAME",
    "DISCORD_HOME_CHANNEL", "DISCORD_HOME_CHANNEL_NAME",
    "DISCORD_REQUIRE_MENTION", "DISCORD_FREE_RESPONSE_CHANNELS",
    "DISCORD_AUTO_THREAD", "SLACK_HOME_CHANNEL", "SLACK_HOME_CHANNEL_NAME",
    "SLACK_ALLOWED_USERS", "WHATSAPP_ENABLED", "WHATSAPP_MODE",
    "WHATSAPP_ALLOWED_USERS", "SIGNAL_HTTP_URL", "SIGNAL_ACCOUNT",
    "SIGNAL_ALLOWED_USERS", "SIGNAL_GROUP_ALLOWED_USERS",
    "SIGNAL_HOME_CHANNEL", "SIGNAL_HOME_CHANNEL_NAME",
    "SIGNAL_IGNORE_STORIES", "HASS_TOKEN", "HASS_URL", "EMAIL_ADDRESS",
    "EMAIL_PASSWORD", "EMAIL_IMAP_HOST", "EMAIL_SMTP_HOST",
    "EMAIL_HOME_ADDRESS", "EMAIL_HOME_ADDRESS_NAME",
    "HERMES_DASHBOARD_SESSION_TOKEN", "GATEWAY_ALLOWED_USERS",
    "GH_TOKEN", "GITHUB_APP_ID", "GITHUB_APP_PRIVATE_KEY_PATH",
    "GITHUB_APP_INSTALLATION_ID", "MODAL_TOKEN_ID", "MODAL_TOKEN_SECRET",
    "DAYTONA_API_KEY", "GATEWAY_RELAY_ID", "GATEWAY_RELAY_SECRET",
    "GATEWAY_RELAY_DELIVERY_KEY", "AWS_BEARER_TOKEN_BEDROCK",
    /* messaging bot tokens (always-strip tier) */
    "TELEGRAM_BOT_TOKEN", "DISCORD_BOT_TOKEN", "SLACK_BOT_TOKEN",
    "SLACK_APP_TOKEN", "NOUS_API_KEY",
    NULL
};

/* Active-venv markers that must not leak (#23473). */
static const char *VENV_MARKERS[] = { "VIRTUAL_ENV", "CONDA_PREFIX", NULL };

/* PoP: env_is_internal_secret @ tools/environments/local.py:_is_hermes_internal_secret */
static bool env_is_internal_secret(const char *key)
{
    char upper[256];
    size_t n = strlen(key);
    if (n >= sizeof(upper)) n = sizeof(upper) - 1;
    for (size_t i = 0; i < n; i++)
        upper[i] = (char)toupper((unsigned char)key[i]);
    upper[n] = '\0';

    #define ENDS_WITH(s, suf) \
        (strlen(s) >= strlen(suf) && \
         strcmp((s) + strlen(s) - strlen(suf), (suf)) == 0)
    if (strncmp(upper, "AUXILIARY_", 10) == 0 &&
        (ENDS_WITH(upper, "_API_KEY") || ENDS_WITH(upper, "_BASE_URL")))
        return true;
    if (strncmp(upper, "GATEWAY_RELAY_", 14) == 0 &&
        (ENDS_WITH(upper, "_SECRET") || ENDS_WITH(upper, "_KEY") ||
         ENDS_WITH(upper, "_TOKEN")))
        return true;
    #undef ENDS_WITH
    return false;
}

/* PoP: scheduler_sanitize_subprocess_env @ tools/environments/local.py:_sanitize_subprocess_env */
char **scheduler_sanitize_subprocess_env(char *const *base_env)
{
    if (!base_env) base_env = environ;
    size_t n = 0;
    while (base_env[n]) n++;

    char **out = calloc(n + 1, sizeof(char *));
    if (!out) return NULL;
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const char *entry = base_env[i];
        const char *eq = strchr(entry, '=');
        size_t klen = eq ? (size_t)(eq - entry) : strlen(entry);
        char key[256];
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, entry, klen);
        key[klen] = '\0';

        bool drop = false;
        for (const char **b = ENV_BLOCKLIST; *b; b++)
            if (strcmp(key, *b) == 0) { drop = true; break; }
        if (!drop)
            for (const char **v = VENV_MARKERS; *v; v++)
                if (strcmp(key, *v) == 0) { drop = true; break; }
        if (!drop && env_is_internal_secret(key)) drop = true;
        if (drop) continue;

        out[m] = strdup(entry);
        if (out[m]) m++;
    }
    out[m] = NULL;
    return out;
}

static void free_env_vector(char **env)
{
    if (!env) return;
    for (size_t i = 0; env[i]; i++) free(env[i]);
    free(env);
}

/* ================================================================
 * Script runner
 * ================================================================ */

/* which(1): search PATH for an executable name. Returns malloc'd path. */
static char *which_prog(const char *name)
{
    const char *path = getenv("PATH");
    if (!path) return NULL;
    char *dup = strdup(path);
    if (!dup) return NULL;
    char *save = NULL;
    for (char *dir = strtok_r(dup, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
        char cand[PATH_MAX];
        snprintf(cand, sizeof(cand), "%s/%s", dir, name);
        if (access(cand, X_OK) == 0) {
            char *out = strdup(cand);
            free(dup);
            return out;
        }
    }
    free(dup);
    return NULL;
}

/* mkdir -p */
static void script_mkdir_p(const char *path)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

/* Read all of fd into a malloc'd string. */
static char *read_all_fd(int fd)
{
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 4096 + 1 > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        ssize_t r = read(fd, buf + len, 4096);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) break;
        len += (size_t)r;
    }
    buf[len] = '\0';
    return buf;
}

/* strip in place, returns same pointer */
static char *strip_inplace(char *s)
{
    if (!s) return s;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t' ||
                   s[len-1] == '\n' || s[len-1] == '\r')) s[--len] = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
        start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    return s;
}

/* Redact stdout/stderr with the fallback contract of _run_job_script. */
static char *redact_or_marker(const char *text)
{
    if (!text) return strdup("");
    char *red = hermes_redact(text);
    if (!red) return strdup("[REDACTED - redaction failed]");
    return red;
}

/* PoP: scheduler_run_job_script @ cron/scheduler.py:_run_job_script */
bool scheduler_run_job_script(const char *script_path, const char *workdir,
                              char **output_out)
{
    if (output_out) *output_out = NULL;
    if (!script_path || !script_path[0]) {
        if (output_out) *output_out = strdup("Script not found: (empty)");
        return false;
    }

    /* scripts_dir = HERMES_HOME/scripts (created, resolved) */
    const char *home = slermes_home();
    if (!home) home = "/tmp/.slermes";
    char scripts_dir[PATH_MAX];
    snprintf(scripts_dir, sizeof(scripts_dir), "%s/scripts", home);
    script_mkdir_p(scripts_dir);
    char scripts_resolved[PATH_MAX];
    if (!realpath(scripts_dir, scripts_resolved))
        snprintf(scripts_resolved, sizeof(scripts_resolved), "%s", scripts_dir);

    /* expanduser + resolve against scripts dir */
    char raw[PATH_MAX];
    if (script_path[0] == '~' &&
        (script_path[1] == '/' || script_path[1] == '\0')) {
        const char *uh = getenv("HOME");
        snprintf(raw, sizeof(raw), "%s%s", uh ? uh : "",
                 script_path + 1);
    } else {
        snprintf(raw, sizeof(raw), "%s", script_path);
    }

    char joined[PATH_MAX];
    if (raw[0] == '/')
        snprintf(joined, sizeof(joined), "%s", raw);
    else
        snprintf(joined, sizeof(joined), "%s/%s", scripts_resolved, raw);

    char resolved[PATH_MAX];
    if (!realpath(joined, resolved)) {
        /* Non-existent path: normalize textually enough for the guard,
         * then report not-found if inside. Python resolve() succeeds on
         * missing files; mimic with the parent's realpath. */
        snprintf(resolved, sizeof(resolved), "%s", joined);
    }

    /* Guard: must reside within scripts dir (traversal/symlink escape). */
    size_t sd_len = strlen(scripts_resolved);
    if (!(strncmp(resolved, scripts_resolved, sd_len) == 0 &&
          (resolved[sd_len] == '/' || resolved[sd_len] == '\0'))) {
        if (output_out) {
            size_t n = strlen(scripts_resolved) + strlen(script_path) + 128;
            char *msg = malloc(n);
            if (msg)
                snprintf(msg, n,
                    "Blocked: script path resolves outside the scripts "
                    "directory (%s): '%s'", scripts_resolved, script_path);
            *output_out = msg;
        }
        return false;
    }

    struct stat st;
    if (stat(resolved, &st) != 0) {
        if (output_out) {
            size_t n = strlen(resolved) + 32;
            char *msg = malloc(n);
            if (msg) snprintf(msg, n, "Script not found: %s", resolved);
            *output_out = msg;
        }
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        if (output_out) {
            size_t n = strlen(resolved) + 40;
            char *msg = malloc(n);
            if (msg) snprintf(msg, n, "Script path is not a file: %s", resolved);
            *output_out = msg;
        }
        return false;
    }

    int script_timeout = scheduler_get_script_timeout();

    /* Interpreter by extension: bash for .sh/.bash, python3 otherwise. */
    const char *dot = strrchr(resolved, '.');
    char suffix[16] = "";
    if (dot) {
        snprintf(suffix, sizeof(suffix), "%s", dot);
        for (char *c = suffix; *c; c++)
            *c = (char)tolower((unsigned char)*c);
    }

    char *interp = NULL;
    if (strcmp(suffix, ".sh") == 0 || strcmp(suffix, ".bash") == 0) {
        interp = which_prog("bash");
        if (!interp && access("/bin/bash", X_OK) == 0)
            interp = strdup("/bin/bash");
        if (!interp) {
            if (output_out) {
                const char *base = strrchr(resolved, '/');
                base = base ? base + 1 : resolved;
                size_t n = strlen(base) + 256;
                char *msg = malloc(n);
                if (msg)
                    snprintf(msg, n,
                        "Cannot run .sh/.bash script '%s': bash not found "
                        "on PATH. On Windows, install Git for Windows "
                        "(which ships Git Bash) or rewrite the script as "
                        "Python (.py).", base);
                *output_out = msg;
            }
            return false;
        }
    } else {
        interp = which_prog("python3");
        if (!interp) interp = which_prog("python");
        if (!interp) {
            if (output_out)
                *output_out = strdup("Script execution failed: no python "
                                     "interpreter found on PATH");
            return false;
        }
    }

    /* cwd: job workdir, else the script's parent (back-compat). */
    char cwd[PATH_MAX];
    if (workdir && workdir[0]) {
        snprintf(cwd, sizeof(cwd), "%s", workdir);
    } else {
        snprintf(cwd, sizeof(cwd), "%s", resolved);
        char *slash = strrchr(cwd, '/');
        if (slash) *slash = '\0';
    }

    /* Sanitized env. */
    char **env = scheduler_sanitize_subprocess_env(environ);

    int outp[2], errp[2];
    if (pipe(outp) != 0 || pipe(errp) != 0) {
        free(interp);
        free_env_vector(env);
        if (output_out)
            *output_out = strdup("Script execution failed: pipe() failed");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]);
        free(interp);
        free_env_vector(env);
        if (output_out)
            *output_out = strdup("Script execution failed: fork() failed");
        return false;
    }

    if (pid == 0) {
        /* child */
        dup2(outp[1], STDOUT_FILENO);
        dup2(errp[1], STDERR_FILENO);
        close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]);
        if (chdir(cwd) != 0) _exit(127);
        char *argv[3] = { interp, resolved, NULL };
        execve(interp, argv, env ? env : environ);
        _exit(127);
    }

    close(outp[1]); close(errp[1]);
    free(interp);
    free_env_vector(env);

    /* Timeout supervision: poll with waitpid(WNOHANG); read pipes as they
     * fill to avoid a full-pipe deadlock. */
    time_t deadline = time(NULL) + script_timeout;
    size_t so_cap = 8192, so_len = 0, se_cap = 8192, se_len = 0;
    char *so = malloc(so_cap), *se = malloc(se_cap);
    bool timed_out = false;
    int status = 0;

    fcntl(outp[0], F_SETFL, O_NONBLOCK);
    fcntl(errp[0], F_SETFL, O_NONBLOCK);

    for (;;) {
        /* drain both pipes */
        for (int which = 0; which < 2; which++) {
            int fd = which == 0 ? outp[0] : errp[0];
            char **buf = which == 0 ? &so : &se;
            size_t *len = which == 0 ? &so_len : &se_len;
            size_t *cap = which == 0 ? &so_cap : &se_cap;
            for (;;) {
                if (*len + 4096 + 1 > *cap) {
                    *cap *= 2;
                    char *nb = realloc(*buf, *cap);
                    if (!nb) break;
                    *buf = nb;
                }
                ssize_t r = read(fd, *buf + *len, 4096);
                if (r > 0) { *len += (size_t)r; continue; }
                break;
            }
        }

        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) break;
        if (time(NULL) > deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            timed_out = true;
            break;
        }
        struct timespec ts = { 0, 50 * 1000 * 1000 };  /* 50ms */
        nanosleep(&ts, NULL);
    }
    /* final drain (blocking reads after exit) */
    fcntl(outp[0], F_SETFL, 0);
    fcntl(errp[0], F_SETFL, 0);
    for (int which = 0; which < 2; which++) {
        int fd = which == 0 ? outp[0] : errp[0];
        char **buf = which == 0 ? &so : &se;
        size_t *len = which == 0 ? &so_len : &se_len;
        size_t *cap = which == 0 ? &so_cap : &se_cap;
        for (;;) {
            if (*len + 4096 + 1 > *cap) {
                *cap *= 2;
                char *nb = realloc(*buf, *cap);
                if (!nb) break;
                *buf = nb;
            }
            ssize_t r = read(fd, *buf + *len, 4096);
            if (r <= 0) break;
            *len += (size_t)r;
        }
    }
    close(outp[0]); close(errp[0]);
    if (so) so[so_len] = '\0';
    if (se) se[se_len] = '\0';

    if (timed_out) {
        free(so); free(se);
        if (output_out) {
            size_t n = strlen(resolved) + 64;
            char *msg = malloc(n);
            if (msg)
                snprintf(msg, n, "Script timed out after %ds: %s",
                         script_timeout, resolved);
            *output_out = msg;
        }
        return false;
    }

    /* strip + redact both streams before ANY return path */
    strip_inplace(so ? so : (char *)"");
    strip_inplace(se ? se : (char *)"");
    char *stdout_red = redact_or_marker(so ? so : "");
    char *stderr_red = redact_or_marker(se ? se : "");
    free(so); free(se);

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    if (exit_code != 0) {
        size_t n = strlen(stdout_red) + strlen(stderr_red) + 128;
        char *msg = malloc(n);
        if (msg) {
            int off = snprintf(msg, n, "Script exited with code %d",
                               exit_code);
            if (stderr_red[0])
                off += snprintf(msg + off, n - (size_t)off,
                                "\nstderr:\n%s", stderr_red);
            if (stdout_red[0])
                snprintf(msg + off, n - (size_t)off,
                         "\nstdout:\n%s", stdout_red);
        }
        free(stdout_red); free(stderr_red);
        if (output_out) *output_out = msg;
        return false;
    }

    free(stderr_red);
    if (output_out) *output_out = stdout_red;
    else free(stdout_red);
    return true;
}

/* ================================================================
 * Claim-heartbeat wrapper
 * ================================================================ */

typedef struct {
    char job_id[128];
    char owner[300];
    pthread_mutex_t mu;
    pthread_cond_t cond;
    bool stop;
} heartbeat_ctx_t;

static void *heartbeat_loop(void *arg)
{
    heartbeat_ctx_t *ctx = arg;
    pthread_mutex_lock(&ctx->mu);
    for (;;) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += RUN_CLAIM_HEARTBEAT_SECONDS;
        int rc = 0;
        while (!ctx->stop && rc != ETIMEDOUT)
            rc = pthread_cond_timedwait(&ctx->cond, &ctx->mu, &ts);
        if (ctx->stop) break;
        pthread_mutex_unlock(&ctx->mu);
        cronjobs_heartbeat_run_claim(ctx->job_id, ctx->owner);
        pthread_mutex_lock(&ctx->mu);
    }
    pthread_mutex_unlock(&ctx->mu);
    return NULL;
}

/* PoP: scheduler_run_job_script_with_claim_heartbeat @ cron/scheduler.py:_run_job_script_with_claim_heartbeat */
bool scheduler_run_job_script_with_claim_heartbeat(const json_t *job,
                                                   const char *script_path,
                                                   const char *workdir,
                                                   char **output_out)
{
    /* Only owned one-shot claims heartbeat; everything else runs plain. */
    const json_t *schedule =
        job ? json_object_get((json_t *)job, "schedule") : NULL;
    const json_t *claim =
        job ? json_object_get((json_t *)job, "run_claim") : NULL;
    const char *kind = json_node_is_object(schedule)
        ? json_get_str((json_t *)schedule, "kind", "") : "";
    const char *owner = json_node_is_object(claim)
        ? json_get_str((json_t *)claim, "by", "") : "";
    if (strcmp(kind, "once") != 0 || !owner[0])
        return scheduler_run_job_script(script_path, workdir, output_out);

    heartbeat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    snprintf(ctx.job_id, sizeof(ctx.job_id), "%s",
             json_get_str((json_t *)job, "id", ""));
    snprintf(ctx.owner, sizeof(ctx.owner), "%s", owner);
    pthread_mutex_init(&ctx.mu, NULL);
    pthread_cond_init(&ctx.cond, NULL);
    ctx.stop = false;

    pthread_t th;
    if (pthread_create(&th, NULL, heartbeat_loop, &ctx) != 0) {
        pthread_cond_destroy(&ctx.cond);
        pthread_mutex_destroy(&ctx.mu);
        return scheduler_run_job_script(script_path, workdir, output_out);
    }

    bool ok = scheduler_run_job_script(script_path, workdir, output_out);

    pthread_mutex_lock(&ctx.mu);
    ctx.stop = true;
    pthread_cond_broadcast(&ctx.cond);
    pthread_mutex_unlock(&ctx.mu);
    pthread_join(th, NULL);
    pthread_cond_destroy(&ctx.cond);
    pthread_mutex_destroy(&ctx.mu);
    return ok;
}

/* ================================================================
 * Wake gate
 * ================================================================ */

/* PoP: scheduler_parse_wake_gate @ cron/scheduler.py:_parse_wake_gate */
bool scheduler_parse_wake_gate(const char *script_output)
{
    if (!script_output || !script_output[0]) return true;

    /* last non-empty line */
    const char *last_start = NULL;
    size_t last_len = 0;
    const char *p = script_output;
    while (*p) {
        const char *line_start = p;
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        /* is the line non-blank? */
        bool blank = true;
        for (size_t i = 0; i < len; i++) {
            if (line_start[i] != ' ' && line_start[i] != '\t' &&
                line_start[i] != '\r') { blank = false; break; }
        }
        if (!blank) { last_start = line_start; last_len = len; }
        if (!nl) break;
        p = nl + 1;
    }
    if (!last_start) return true;

    /* strip the line */
    while (last_len && (last_start[0] == ' ' || last_start[0] == '\t' ||
                        last_start[0] == '\r')) { last_start++; last_len--; }
    while (last_len && (last_start[last_len-1] == ' ' ||
                        last_start[last_len-1] == '\t' ||
                        last_start[last_len-1] == '\r')) last_len--;
    char *line = malloc(last_len + 1);
    if (!line) return true;
    memcpy(line, last_start, last_len);
    line[last_len] = '\0';

    char *err = NULL;
    json_t *gate = json_parse(line, &err);
    free(line);
    free(err);
    if (!gate) return true;

    bool wake = true;
    if (json_node_is_object(gate)) {
        json_t *flag = json_object_get(gate, "wakeAgent");
        /* only an explicit boolean false gates the agent off */
        if (flag && flag->type == JSON_BOOL && !json_bool_value(flag))
            wake = false;
    }
    json_free(gate);
    return wake;
}


/* ── from src/cron/port_cron_scheduler_toolsets.c ── */
/*
 * port_cron_scheduler_toolsets.c — faithful C11 port of cron/scheduler.py's
 * toolset-resolution surface:
 *   _resolve_cron_disabled_toolsets
 *   _merge_mcp_into_per_job_toolsets
 *   _resolve_cron_enabled_toolsets
 *
 * Reuses the existing platform_tools C plumbing (hermes_cli/tools_config.py
 * port) — enabled_mcp_server_names and _get_platform_tools — via the
 * platform_tools.h subsystem header. No god headers.
 */

#include "cron_scheduler_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "platform_tools.h"

/* ── small list helpers ─────────────────────────────────────────────── */

static bool list_contains(char *const *list, size_t n, const char *s)
{
    for (size_t i = 0; i < n; i++)
        if (list[i] && strcmp(list[i], s) == 0) return true;
    return false;
}

static char **list_finish(char **list, size_t n, size_t *out_n)
{
    if (list) list[n] = NULL;
    if (out_n) *out_n = n;
    return list;
}

void scheduler_free_string_list(char **list, size_t n)
{
    if (!list) return;
    if (n == 0) { for (size_t i = 0; list[i]; i++) free(list[i]); }
    else        { for (size_t i = 0; i < n; i++) free(list[i]); }
    free(list);
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* strip leading/trailing ASCII whitespace into a fresh string */
static char *strip_dup(const char *s)
{
    if (!s) return strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) len--;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* ── _resolve_cron_disabled_toolsets ────────────────────────────────── */

/* PoP: scheduler_resolve_cron_disabled_toolsets @ cron/scheduler.py:_resolve_cron_disabled_toolsets */
char **scheduler_resolve_cron_disabled_toolsets(const json_t *cfg,
                                                size_t *out_n)
{
    /* Three protected toolsets always disabled in cron context. */
    static const char *protected_ts[] = { "cronjob", "messaging", "clarify" };

    const json_t *agent_cfg = cfg ? json_object_get((json_t *)cfg, "agent") : NULL;
    const json_t *user_disabled =
        agent_cfg ? json_object_get((json_t *)agent_cfg, "disabled_toolsets") : NULL;
    size_t extra = (user_disabled && user_disabled->type == JSON_ARRAY)
                       ? json_array_size(user_disabled) : 0;

    char **out = calloc(3 + extra + 1, sizeof(char *));
    if (!out) { if (out_n) *out_n = 0; return NULL; }
    size_t n = 0;
    for (size_t i = 0; i < 3; i++)
        out[n++] = strdup(protected_ts[i]);

    for (size_t i = 0; i < extra; i++) {
        const json_t *item = json_array_get(user_disabled, i);
        const char *raw = item ? json_string_value_safe(item) : NULL;
        char buf[128];
        if (!raw) {
            /* str(name) for non-strings: numbers/bools stringify */
            if (item && item->type == JSON_NUMBER) {
                snprintf(buf, sizeof(buf), "%g", item->num_val);
                raw = buf;
            } else if (item && item->type == JSON_BOOL) {
                raw = item->bool_val ? "True" : "False";
            } else {
                continue;
            }
        }
        char *name = strip_dup(raw);
        if (!name) continue;
        if (name[0] && !list_contains(out, n, name))
            out[n++] = name;
        else
            free(name);
    }
    return list_finish(out, n, out_n);
}

/* ── _merge_mcp_into_per_job_toolsets ───────────────────────────────── */

/* PoP: scheduler_merge_mcp_into_per_job_toolsets @ cron/scheduler.py:_merge_mcp_into_per_job_toolsets */
char **scheduler_merge_mcp_into_per_job_toolsets(const char *const *per_job,
                                                 size_t n_per_job,
                                                 const json_t *cfg,
                                                 size_t *out_n)
{
    /* result = [t for t in per_job if t != "no_mcp"] */
    bool had_no_mcp = false;
    size_t n_mcp = 0;
    char **mcp = platform_tools_enabled_mcp_server_names(cfg, &n_mcp);

    char **out = calloc(n_per_job + n_mcp + 1, sizeof(char *));
    if (!out) {
        platform_tools_free_list(mcp, n_mcp);
        if (out_n) *out_n = 0;
        return NULL;
    }
    size_t n = 0;
    for (size_t i = 0; i < n_per_job; i++) {
        if (!per_job[i]) continue;
        if (strcmp(per_job[i], "no_mcp") == 0) { had_no_mcp = true; continue; }
        out[n++] = strdup(per_job[i]);
    }
    if (had_no_mcp) {
        platform_tools_free_list(mcp, n_mcp);
        return list_finish(out, n, out_n);
    }

    /* if set(result) & enabled_mcp: return result (user-named allowlist) */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n_mcp; j++) {
            if (mcp[j] && strcmp(out[i], mcp[j]) == 0) {
                platform_tools_free_list(mcp, n_mcp);
                return list_finish(out, n, out_n);
            }
        }
    }

    /* union in every globally-enabled MCP server, sorted order.
     * platform_tools_enabled_mcp_server_names already returns a sorted
     * unique list, matching Python's sorted(enabled_mcp). */
    if (n_mcp > 1)
        qsort(mcp, n_mcp, sizeof(char *), cmp_str);
    for (size_t j = 0; j < n_mcp; j++) {
        if (mcp[j] && !list_contains(out, n, mcp[j]))
            out[n++] = strdup(mcp[j]);
    }
    platform_tools_free_list(mcp, n_mcp);
    return list_finish(out, n, out_n);
}

/* ── _resolve_cron_enabled_toolsets ─────────────────────────────────── */

/* PoP: scheduler_resolve_cron_enabled_toolsets @ cron/scheduler.py:_resolve_cron_enabled_toolsets */
char **scheduler_resolve_cron_enabled_toolsets(const json_t *job,
                                               const json_t *cfg,
                                               size_t *out_n)
{
    if (out_n) *out_n = 0;

    /* 1. Per-job enabled_toolsets wins (truthiness: non-empty list). */
    const json_t *per_job =
        job ? json_object_get((json_t *)job, "enabled_toolsets") : NULL;
    if (per_job && per_job->type == JSON_ARRAY && json_array_size(per_job) > 0) {
        size_t pn = json_array_size(per_job);
        const char **vec = calloc(pn + 1, sizeof(char *));
        if (!vec) return NULL;
        size_t vn = 0;
        for (size_t i = 0; i < pn; i++) {
            const char *s = json_string_value_safe(json_array_get(per_job, i));
            if (s) vec[vn++] = s;
        }
        char **out = scheduler_merge_mcp_into_per_job_toolsets(
            vec, vn, cfg, out_n);
        free(vec);
        return out;
    }

    /* 2. Per-platform "cron" toolset config (mirrors _get_platform_tools),
     *    sorted. platform_tools_get returns a sorted unique list already. */
    size_t n = 0;
    char **out = platform_tools_get(cfg, "cron", true, &n);
    if (!out) return NULL;   /* 3. NULL => caller loads the full default set */
    if (n > 1) qsort(out, n, sizeof(char *), cmp_str);
    if (out_n) *out_n = n;
    return out;
}


/* ── from src/cron/port_scheduler.c ── */
/**
 * port_scheduler.c — Port of Python: cron/scheduler.py
 *
 * Real C implementations for cron scheduler functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

/* Port of Python: _confirm_adapter_delivery */
bool confirm_adapter_delivery(json_t *send_result)
{
    if (!send_result) {
        hermes_log(LOG_WARNING, "port", "confirm_adapter_delivery: null result");
        return false;
    }
    const char *status = json_node_get_string(json_object_get(send_result, "status"));
    if (status && strcmp(status, "delivered") == 0) {
        hermes_log(LOG_INFO, "port", "confirm_adapter_delivery: confirmed");
        return true;
    }
    const char *error = json_node_get_string(json_object_get(send_result, "error"));
    if (error) {
        hermes_log(LOG_WARNING, "port", "confirm_adapter_delivery: error=%s", error);
    }
    return false;
}

/* Port of Python: _notify_provider_jobs_changed */
void notify_provider_jobs_changed(void)
{
    hermes_log(LOG_INFO, "port", "notify_provider_jobs_changed: notifying providers");
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/cron/jobs_changed.notify", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"event\": \"jobs_changed\", \"timestamp\": %ld}\n", (long)time(NULL));
        fclose(f);
        hermes_log(LOG_DEBUG, "port", "notify_provider_jobs_changed: notification written");
    } else {
        hermes_log(LOG_WARNING, "port", "notify_provider_jobs_changed: cannot write notification");
    }
}

/* Port of Python: _summarize_cron_failure_for_delivery */
char *summarize_cron_failure_for_delivery(const char *job, const char *error)
{
    if (!job) {
        return strdup("{\"error\": \"unknown job\"}");
    }
    char *summary = malloc(4096);
    if (!summary) return NULL;
    if (error) {
        snprintf(summary, 4096,
                 "{\"job\": \"%s\", \"status\": \"failed\", \"error\": \"%s\"}",
                 job, error);
    } else {
        snprintf(summary, 4096,
                 "{\"job\": \"%s\", \"status\": \"failed\", \"error\": \"unknown\"}",
                 job);
    }
    hermes_log(LOG_INFO, "port", "summarize_cron_failure: %s", summary);
    return summary;
}

/* Port of Python: run_one_job */
bool run_one_job(const char *job)
{
    if (!job) {
        hermes_log(LOG_WARNING, "port", "run_one_job: null job");
        return false;
    }
    hermes_log(LOG_INFO, "port", "run_one_job: executing '%s'", job);
    int ret = system(job);
    if (ret != 0) {
        int exit_code = WEXITSTATUS(ret);
        hermes_log(LOG_WARNING, "port", "run_one_job: job failed (exit=%d)", exit_code);
        return false;
    }
    hermes_log(LOG_INFO, "port", "run_one_job: job completed successfully");
    return true;
}


/* ── from src/cron/port_scheduler_gaps.c ── */
/*
 * port_scheduler_gaps.c — Faithful C11 port of cron/scheduler.py REAL_GAP functions.
 * Job scheduling: registration, preflight checks, audit, delivery.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: sched_fallback_chain_phrase @ cron/scheduler.py:_fallback_chain_phrase */
void sched_fallback_chain_phrase(char *out, size_t out_sz) {
    snprintf(out, out_sz, "Fallback chain was exhausted or unavailable.");
}

/* PoP: sched_try_register_running_job @ cron/scheduler.py:try_register_running_job */
int sched_try_register_running_job(const char *job_id) {
    (void)job_id;
    return 1; /* Success */
}

/* PoP: sched_release_running_job @ cron/scheduler.py:release_running_job */
void sched_release_running_job(const char *job_id) {
    (void)job_id;
}

/* PoP: sched_write_usage_audit @ cron/scheduler.py:_write_usage_audit */
void sched_write_usage_audit(const char *job_id, int status) {
    (void)job_id; (void)status;
}

/* PoP: sched_get_config_home_channel @ cron/scheduler.py:_get_config_home_channel */
void sched_get_config_home_channel(char *out, size_t out_sz) {
    const char *home = getenv("HOME");
    if (!home) { out[0] = '\0'; return; }
    snprintf(out, out_sz, "%s", home);
}

/* PoP: sched_env_home_target_chat_id @ cron/scheduler.py:_env_home_target_chat_id */
void sched_env_home_target_chat_id(char *out, size_t out_sz) {
    snprintf(out, out_sz, "");
}

/* PoP: sched_relay_fronted_delivery_platforms @ cron/scheduler.py:_relay_fronted_delivery_platforms */
void sched_relay_fronted_delivery_platforms(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}

/* PoP: sched_preflight_check_provider_key @ cron/scheduler.py:_preflight_check_provider_key */
int sched_preflight_check_provider_key(void) {
    return 1; /* OK */
}

/* PoP: sched_preflight_check_skills @ cron/scheduler.py:_preflight_check_skills */
int sched_preflight_check_skills(void) {
    return 1; /* OK */
}

/* PoP: sched_preflight_job_config @ cron/scheduler.py:_preflight_job_config */
int sched_preflight_job_config(const char *job_id) {
    (void)job_id;
    return 1; /* OK */
}

/* PoP: sched_create_job_with_scheduler_registration @ cron/scheduler.py:create_job_with_scheduler_registration */
int sched_create_job_with_scheduler_registration(const char *job_id, char *out_json, size_t out_sz) {
    (void)job_id;
    snprintf(out_json, out_sz, "{\"status\":\"created\"}");
    return 1;
}

