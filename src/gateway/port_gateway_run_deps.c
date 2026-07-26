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

#include "sqlite3.h"
#include "slermes_home.h"
#include "goal_contract.h"
#include "gateway_status.h"
#include "hermes_json.h"
#include "yaml.h"
#include "fallback_config_helpers.h"

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

    char *json_str = yaml_to_json_string(doc, NULL);
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
