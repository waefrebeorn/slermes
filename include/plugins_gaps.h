/* plugins_gaps.h — C11 port of the remaining REAL_GAP surface of
 * hermes_cli/plugins.py (PluginRegistration / PluginState / PluginContext /
 * PluginManager runtime) and hermes_cli/plugins_cmd.py helpers.
 *
 * Opaque-ish structs, C11 only, pthread-based event bus. No god header.
 */
#ifndef SLERMES_PLUGINS_GAPS_H
#define SLERMES_PLUGINS_GAPS_H

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── constants (mirrors of module-level Python constants) ─────────────── */
#define PLG_ENTRY_POINTS_GROUP                    "hermes_agent.plugins"
#define PLG_ENTRY_POINT_CAPABILITIES_GROUP        "hermes_agent.plugin_capabilities"
#define PLG_HERMES_EVENT_NAMESPACE                "hermes"
#define PLG_NS_PARENT                             "hermes_plugins"
#define PLG_MAX_SYSTEM_PROMPT_SECTIONS            32
#define PLG_MAX_SECTIONS_TOTAL_CHARS              8192
#define PLG_PLUGIN_STATE_QUOTA_BYTES              (10 * 1024 * 1024)
#define PLG_MCP_RESULT_CHAR_CAP                   65536
#define PLG_EVENT_PENDING_CAP                     64
#define PLG_EVENT_EMIT_DEPTH_CAP                  8

/* Callback shape: receives the hook payload as a JSON object (borrowed),
 * returns a fresh JSON value or NULL (Python None). */
typedef json_t *(*plug_cb_fn)(const json_t *payload);

/* ── manifest / registration / loaded plugin ──────────────────────────── */

typedef struct plug_manifest {
    char *name;
    char *key;
    char *kind;
    char *version;
    char *description;
    char *source;          /* "bundled" | "user" | "project" | "entrypoint" */
    char *path;
    char *skill_namespace;
    bool  portable;
    bool  provides_tools;
    json_t *capabilities;       /* array of strings; owned; may be NULL */
    json_t *config_schema;      /* owned; may be NULL */
    char **python_dependencies; /* owned */
    size_t n_python_dependencies;
} plug_manifest_t;

typedef struct plug_registration {
    long   id;
    char  *kind;
    char  *key;
    char  *plugin_key;
    bool   persistent;
    bool   disposed;
    void (*release)(void *arg);
    void  *release_arg;
} plug_registration_t;

typedef struct plug_loaded {
    plug_manifest_t manifest;
    bool    enabled;
    char   *error;
    char  **tools_registered;      size_t n_tools_registered;
    char  **hooks_registered;      size_t n_hooks_registered;
    char  **middleware_registered; size_t n_middleware_registered;
    char  **commands_registered;   size_t n_commands_registered;
    char   *module_name;           /* loaded module stand-in (path or dlopen ns) */
} plug_loaded_t;

/* ── event subscriptions & sections ───────────────────────────────────── */

typedef struct plug_subscription {
    long        id;
    char       *owner;
    char       *event;
    plug_cb_fn  callback;
    char       *callback_name;
} plug_subscription_t;

typedef struct plug_section {
    char  *id;
    char  *plugin;
    char  *position;
    int    max_chars;
    plug_cb_fn content_fn;      /* when callable(content) */
    char  *content_static;      /* when plain string */
} plug_section_t;

typedef struct plug_approval_transport {
    char       *name;
    plug_cb_fn  present;
    char       *plugin_id;
    char       *profile_home;
} plug_approval_transport_t;

/* queued plugin event */
typedef struct plug_queued_event {
    char   *event;
    json_t *payload;             /* owned snapshot */
    long   *sub_ids;             /* snapshot of subscription ids */
    size_t  n_subs;
    int     depth;
    long    generation;
} plug_queued_event_t;

/* ── PluginManager ────────────────────────────────────────────────────── */

typedef struct plug_ledger_entry {
    char                 *key;
    plug_registration_t **items;
    size_t                n;
} plug_ledger_entry_t;

typedef struct plug_hook_list {
    char       *hook;
    plug_cb_fn *cbs;
    char      **names;
    size_t      n;
} plug_hook_list_t;

struct plug_manager {
    pthread_mutex_t discovery_lock;
    bool   discovered;
    char  *home_path;
    char  *scope_key;

    /* _plugins */
    plug_loaded_t **plugins;         size_t n_plugins;
    /* _registration_order */
    plug_registration_t **reg_order; size_t n_reg_order;
    /* _ownership_ledger */
    plug_ledger_entry_t *ledger;     size_t n_ledger;
    /* _persistent_carryover */
    plug_registration_t **carryover; size_t n_carryover;

    /* _hooks */
    plug_hook_list_t *hooks;         size_t n_hooks;

    /* name sets */
    char **plugin_tool_names;     size_t n_plugin_tool_names;
    char **plugin_platform_names; size_t n_plugin_platform_names;
    char **cli_commands;          size_t n_cli_commands;

    /* system prompt sections */
    plug_section_t *sections;        size_t n_sections;

    /* approval transports */
    plug_approval_transport_t *transports; size_t n_transports;

    /* gateway message injector: owner pointer + fn(payload)->bool */
    void      *gateway_injector_owner;
    bool     (*gateway_injector)(void *owner, const json_t *kwargs);
    void      *context_engine;

    /* event bus */
    pthread_mutex_t event_lock;
    pthread_cond_t  event_idle_cond;
    plug_subscription_t *subs;          size_t n_subs;
    long   event_generation;
    long  *pending_gens;                size_t n_pending_gens; /* parallel counts */
    long  *pending_counts;
    plug_queued_event_t queue[PLG_EVENT_PENDING_CAP];
    size_t q_head, q_len;
    pthread_t event_worker;
    bool   event_worker_started;
    int    emit_depth;                  /* guarded by event_lock (worker hop) */
};

typedef struct plug_manager plug_manager_t;

/* ── PluginState / PluginContext ──────────────────────────────────────── */

typedef struct PluginState {
    char *plugin_id;
    char *data_namespace;
} PluginState;

typedef struct PlatformActions {
    char *plugin_id;
} PlatformActions;

typedef struct PluginContext {
    plug_manifest_t manifest;
    plug_manager_t *manager;
    PluginState    *_state;
    PlatformActions *_platform_actions;
    char           *_memory_provider;   /* provider name once registered */
} PluginContext;

/* ── manager lifecycle (support surface used by the ports) ────────────── */
plug_manager_t *plugins_get_plugin_manager(void);      /* get_plugin_manager() */
char *plugins_effective_home(void);                    /* override || home */
const char *_plugin_home_scope(const char *home);      /* returns saved token */
void _plugin_home_scope_reset(const char *token);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PLUGINS_GAPS_H */
