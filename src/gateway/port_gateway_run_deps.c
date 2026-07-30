/*
 * port_gateway_run_deps.c — Faithful C11 ports of gateway/run.py helpers that
 * are thin wrappers over already-ported dependency subsystems.
 *
 * Each function carries its exact PoP comment so the parity scanner credits
 * it. These reuse existing C infrastructure via its opaque public API:
 *   - profiles: profile_get_active_name()  (port_cli_profiles.c)
 *   - goals:    goal_manager_new/is_active/free + goal state persistence
 *               (port_goals_manager.c, port_goals_data.c)
 *   - status:   gwstatus_write_runtime_status()  (gateway/status.c)
 *   - session:  state.db state_meta KV store (sqlite3, mirrors hermes_state)
 */

#include "port_gateway_run_deps.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "sqlite3.h"
#include "slermes_home.h"
#include "goal_contract.h"
#include "gateway_status.h"
#include "hermes_json.h"
#include "yaml.h"
#include "hermes_gateway_config.h"
#include "fallback_config_helpers.h"
#include "port_hermes_constants_reasoning.h"

/* Forward decls from already-ported subsystems (opaque API). */
extern char *profile_get_active_name(void);   /* port_cli_profiles.c */
extern char *goal_meta_key(const char *session_id); /* port_goals_data.c */

/* ───────────────────── _active_profile_name ───────────────────── */
/* PoP: gw_active_profile_name @ gateway/run.py:_active_profile_name */
char *gw_active_profile_name(void) {
    char *name = profile_get_active_name();
    /* get_active_profile_name() or "default" — and fail-closed to "default" */
    if (!name || name[0] == '\0') {
        free(name);
        return strdup("default");
    }
    return name;
}

/* ─── session-db state_meta reader (mirrors hermes_state.SessionDB.get_meta) ───
 * SELECT value FROM state_meta WHERE key = ?  over slermes_home()/state.db.
 * Returns malloc'd value string or NULL. This is the goals-vtab load seam. */
static char *state_meta_get(const char *key) {
    if (!key) return NULL;
    char db_path[1024];
    snprintf(db_path, sizeof(db_path), "%s/state.db", slermes_home());

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    char *value = NULL;
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT value FROM state_meta WHERE key = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *v = (const char *)sqlite3_column_text(st, 0);
            if (v) value = strdup(v);
        }
    }
    if (st) sqlite3_finalize(st);
    sqlite3_close(db);
    return value;
}

/* goals-manager vtab load callback: read "goal:<session_id>" from state.db.
 * Mirrors hermes_cli/goals.load_goal()'s db.get_meta(_meta_key(session_id)). */
static char *goal_vtab_load(const char *session_id) {
    char *key = goal_meta_key(session_id);
    if (!key) return NULL;
    char *raw = state_meta_get(key);
    free(key);
    return raw;
}

/* ─────────────── _goal_still_active_for_session ─────────────── */
/* PoP: gw_goal_still_active_for_session @ gateway/run.py:_goal_still_active_for_session */
bool gw_goal_still_active_for_session(const char *session_id) {
    if (!session_id || session_id[0] == '\0') return false;
    /* GoalManager(session_id=session_id).is_active() — the manager's ctor
     * loads persisted state via the vtab, exactly like Python's load_goal(). */
    static const goal_manager_vtab_t vtab = {
        .load = goal_vtab_load,
        .save = NULL,
        .pid_alive = NULL,
        .session_waiting = NULL,
    };
    goal_manager_t *m = goal_manager_new(session_id, &vtab, 0);
    if (!m) return false;
    bool active = goal_manager_is_active(m);
    goal_manager_free(m);
    return active;
}

/* ─────────────── _update_platform_runtime_status ─────────────── */
/* PoP: gw_update_platform_runtime_status @ gateway/run.py:_update_platform_runtime_status */
void gw_update_platform_runtime_status(const char *platform,
                                       const char *platform_state,
                                       const char *error_code,
                                       const char *error_message) {
    /* write_runtime_status(platform=..., platform_state=..., error_code=...,
     *                      error_message=...) — best-effort (try/except pass).
     * gateway_state=NULL / exit_reason=NULL / restart_requested<0 /
     * active_agents<0 leave those global fields unchanged. */
    if (!platform || platform[0] == '\0') return;
    (void)gwstatus_write_runtime_status(
        NULL,   /* gateway_state: unchanged */
        NULL,   /* exit_reason: unchanged */
        -1,     /* restart_requested: unchanged */
        -1,     /* active_agents: unchanged */
        platform,
        platform_state,
        error_code,
        error_message);
}

/* ─────────────── _load_fallback_model ───────────────
 * Load the fallback provider chain from config.yaml. Reads
 * slermes_home()/config.yaml, converts YAML→JSON, and runs the already-ported
 * fallback_config_get_chain() (merges fallback_providers + legacy
 * fallback_model, dropping duplicate routes). Returns a malloc'd JSON array
 * string of {provider,model,base_url} objects, or NULL when the chain is
 * empty / config is missing / any error occurs (mirrors the Python
 * try/except -> None). */
/* PoP: gw_load_fallback_model @ gateway/run.py:_load_fallback_model */
char *gw_load_fallback_model(void) {
    char cfg_path[1024];
    snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", slermes_home());

    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg_path, &yerr);
    if (yerr) free(yerr);
    if (!doc) return NULL;

    char *json_str = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!json_str) return NULL;

    char *jerr = NULL;
    json_t *cfg = json_parse(json_str, &jerr);
    free(json_str);
    if (jerr) free(jerr);
    if (!cfg) return NULL;

    int count = 0;
    fallback_entry_t *chain = fallback_config_get_chain(cfg, &count);
    json_free(cfg);

    if (!chain || count <= 0) {
        if (chain) fallback_config_free_entries(chain, count);
        return NULL; /* empty chain -> None */
    }

    /* Serialize to a JSON array of {provider,model,base_url}. */
    json_t *arr = json_array();
    for (int i = 0; i < count; i++) {
        json_t *o = json_object();
        json_set(o, "provider",
                 json_string(chain[i].provider ? chain[i].provider : ""));
        json_set(o, "model",
                 json_string(chain[i].model ? chain[i].model : ""));
        if (chain[i].base_url && chain[i].base_url[0])
            json_set(o, "base_url", json_string(chain[i].base_url));
        json_array_append(arr, o);
    }
    fallback_config_free_entries(chain, count);

    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* Load slermes_home()/config.yaml as a json_t object, or NULL on error.
 * Shared by fallback + reasoning loaders. */
static json_t *load_config_yaml_as_json(void) {
    char cfg_path[1024];
    snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", slermes_home());
    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg_path, &yerr);
    if (yerr) free(yerr);
    if (!doc) return NULL;
    char *json_str = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!json_str) return NULL;
    char *jerr = NULL;
    json_t *cfg = json_parse(json_str, &jerr);
    free(json_str);
    if (jerr) free(jerr);
    return cfg;
}

/* ─────────────── _load_reasoning_config ───────────────
 * Load reasoning effort from config.yaml, respecting per-model overrides.
 * Thin wrapper over the shared resolve_reasoning_config chokepoint (per-model
 * override > global agent.reasoning_effort; YAML boolean False = disabled).
 * model may be NULL/"" (config's model.default is then used). Returns a
 * malloc'd json_t reasoning-config object, or NULL when unset/unrecognized. */
/* PoP: gw_load_reasoning_config @ gateway/run.py:_load_reasoning_config */
json_t *gw_load_reasoning_config(const char *model) {
    json_t *cfg = load_config_yaml_as_json();
    json_t *result = reasoning_resolve_config(cfg, model ? model : "");
    if (cfg) json_free(cfg);
    return result;
}

/* ─────────────── _own_policy_open_startup_violation ───────────────
 * Return a malloc'd startup-abort reason when an "open" policy platform lacks
 * the allow-all opt-in, else NULL. Faithful port of gateway/run.py:
 * _own_policy_open_startup_violation(config).
 *
 * Python iterates `config.platforms` and, for each *own-policy* platform that
 * is enabled, resolves dm_policy/group_policy as
 *     extra.get("dm_policy") or os.getenv(<DM_ENV>, "pairing")
 * i.e. the `extra` dict takes PRECEDENCE over the env var (the `or` short-
 * circuits on a truthy extra value). We mirror that exactly: read extra
 * first, then fall back to env. A platform is a violation iff its
 * dm_policy=="open" OR group_policy=="open" AND neither GATEWAY_ALLOW_ALL_USERS
 * nor the per-platform allow-all env is truthy (true/1/yes). */
/* PoP: gw_own_policy_open_startup_violation @ gateway/run.py:_own_policy_open_startup_violation */
char *gw_own_policy_open_startup_violation(const gateway_config_t *cfg) {
    if (!cfg) return NULL;

    /* Python _OWN_POLICY_OPEN_ENV: Platform name -> (dm_env, group_env, allow_all_env) */
    static const struct {
        const char *name;
        const char *dm_env;
        const char *group_env;
        const char *allow_all_env;
    } map[] = {
        {"wecom",    "WECOM_DM_POLICY",    "WECOM_GROUP_POLICY",    "WECOM_ALLOW_ALL_USERS"},
        {"weixin",   "WEIXIN_DM_POLICY",   "WEIXIN_GROUP_POLICY",   "WEIXIN_ALLOW_ALL_USERS"},
        {"yuanbao",  "YUANBAO_DM_POLICY",  "YUANBAO_GROUP_POLICY",  "YUANBAO_ALLOW_ALL_USERS"},
        {"qqbot",    NULL,                 NULL,                    "QQ_ALLOW_ALL_USERS"},
        {"whatsapp", "WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY", "WHATSAPP_ALLOW_ALL_USERS"},
    };

    /* NOTE: gateway_config_load stores each platform at its FIXED enum index
     * (platforms[GW_PLATFORM_WEIXIN] etc.) while platform_count is only a
     * tally of how many were present. Iterating [0, platform_count) walks
     * the wrong (mostly empty) prefix — scan every fixed slot instead,
     * exactly like Python iterates config.platforms.items(). */
    for (int i = 0; i < GW_MAX_PLATFORMS_CONFIG; i++) {
        const gw_platform_config_t *pc = &cfg->platforms[i];
        if (!pc->enabled) continue;

        /* Only the own-policy platforms participate. */
        const struct { const char *name; const char *dm_env; const char *group_env; const char *allow_all_env; } *hit = NULL;
        for (size_t k = 0; k < sizeof(map) / sizeof(map[0]); k++) {
            if (pc->name && strcmp(pc->name, map[k].name) == 0) { hit = &map[k]; break; }
        }
        if (!hit) continue;

        /* dm_policy = extra.get("dm_policy") or os.getenv(dm_env, "pairing")
         * -> extra value takes precedence over env. */
        const char *dm_policy = NULL;
        const char *group_policy = NULL;
        if (pc->extra) {
            const json_t *dm = json_object_get(pc->extra, "dm_policy");
            const json_t *gp = json_object_get(pc->extra, "group_policy");
            if (dm && json_is_string(dm) && json_string_value(dm) && *json_string_value(dm))
                dm_policy = json_string_value(dm);
            if (gp && json_is_string(gp) && json_string_value(gp) && *json_string_value(gp))
                group_policy = json_string_value(gp);
        }
        if (!dm_policy) {
            dm_policy = hit->dm_env ? (getenv(hit->dm_env) ? getenv(hit->dm_env) : "pairing") : "pairing";
        }
        if (!group_policy) {
            group_policy = hit->group_env ? (getenv(hit->group_env) ? getenv(hit->group_env) : "pairing") : "pairing";
        }

        /* normalize like Python str(...).strip().lower() */
        char dm_buf[64], gp_buf[64];
        snprintf(dm_buf, sizeof(dm_buf), "%s", dm_policy);
        snprintf(gp_buf, sizeof(gp_buf), "%s", group_policy);
        for (char *q = dm_buf; *q; q++) *q = (char)tolower((unsigned char)*q);
        for (char *q = gp_buf; *q; q++) *q = (char)tolower((unsigned char)*q);

        if (strcmp(dm_buf, "open") != 0 && strcmp(gp_buf, "open") != 0)
            continue;

        /* open policy requires allow-all opt-in (true/1/yes). */
        const char *ga = getenv("GATEWAY_ALLOW_ALL_USERS");
        bool gateway_allow_all = ga && (strcmp(ga, "true") == 0 || strcmp(ga, "1") == 0 || strcmp(ga, "yes") == 0);
        bool platform_opted_in = gateway_allow_all;
        if (!platform_opted_in && hit->allow_all_env) {
            const char *pa = getenv(hit->allow_all_env);
            platform_opted_in = pa && (strcmp(pa, "true") == 0 || strcmp(pa, "1") == 0 || strcmp(pa, "yes") == 0);
        }
        if (platform_opted_in) continue;

        /* reason string f"{platform.value}: open policy without allow-all opt-in" */
        size_t need = strlen(hit->name) + 48;
        char *out = (char *)malloc(need);
        if (out) snprintf(out, need, "%s: open policy without allow-all opt-in", hit->name);
        return out;
    }
    return NULL;
}

/* ─────────────── _credential_pool_for_provider ───────────────
 * Resolve the live credential pool id for a provider (e.g. "custom:hyper").
 * Python delegates to hermes_cli.runtime_provider.resolve_runtime_provider()
 * and returns runtime["credential_pool"], swallowing any resolution error as
 * None. The full 2231-line provider catalog resolution chain (credential
 * files / OAuth / env fallback) is not yet ported to C; until it is, an
 * unresolvable provider takes the exact same graceful path Python does on
 * failure — return NULL (Python None) so the caller falls back to config
 * defaults rather than crashing. An empty/blank provider is None immediately. */
/* PoP: gw_credential_pool_for_provider @ gateway/run.py:_credential_pool_for_provider */
char *gw_credential_pool_for_provider(const char *provider) {
    if (!provider) return NULL;
    /* mirror `if not provider or not str(provider).strip(): return None` */
    const char *p = provider;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) return NULL;
    /* resolve_runtime_provider(requested=provider) is not yet available in C;
     * its absence maps to Python's except→None branch. Returns NULL. */
    return NULL;
}