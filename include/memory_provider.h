/**
 * @defgroup memory_provider Memory Provider Plugin Interface
 * @brief Abstract interface for pluggable memory providers.
 *
 * Memory providers give the agent persistent recall across sessions.
 * Only one external provider runs at a time.
 *
 * Lifecycle (called by MemoryManager):
 *   is_available()          — check config/credentials
 *   initialize()            — connect, create resources
 *   system_prompt_block()   — static text for system prompt
 *   prefetch(query)         — background recall before each turn
 *   queue_prefetch(query)   — queue background recall for next turn
 *   sync_turn(user, asst)   — async write after each turn
 *   get_tool_schemas()      — tool schemas to expose
 *   handle_tool_call()      — dispatch a tool call
 *   shutdown()              — clean exit
 *
 * Optional hooks (default no-op):
 *   on_turn_start()         — per-turn tick
 *   on_session_end()        — end-of-session extraction
 *   on_session_switch()     — mid-process session_id rotation
 *   on_pre_compress()       — extract before context compression
 *   on_delegation()         — parent-side observation of subagent work
 *   get_config_schema()     — config fields for setup
 *   save_config()           — write non-secret config
 *   on_memory_write()       — mirror built-in memory writes
 *
 * @{
 */
#ifndef MEMORY_PROVIDER_H
#define MEMORY_PROVIDER_H

#include "hermes_core_types.h"
#include "hermes_json.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct memory_provider_t memory_provider_t;
typedef struct memory_provider_vtable_t memory_provider_vtable_t;

/* Config field descriptor for 'hermes memory setup' */
typedef struct {
    char key[128];
    char description[512];
    bool secret;
    bool required;
    char default_val[256];
    const char **choices;   /* NULL-terminated array, or NULL */
    int  choices_count;
    char url[512];
    char env_var[128];
} memory_provider_config_field_t;

/* ================================================================
 *  Memory Provider VTable (function pointer struct)
 * ================================================================ */

struct memory_provider_vtable_t {
    /* Provider name (e.g. "builtin", "honcho", "hindsight") */
    const char *name;

    /* -- Core lifecycle (must implement) ---------------------------- */

    /* Return true if provider is configured and ready.
     * Should not make network calls — just check config/deps. */
    bool (*is_available)(memory_provider_t *self);

    /* Initialize for a session. May create resources, connect, etc.
     * hermes_home: active HERMES_HOME path.
     * platform: "cli", "telegram", "discord", "cron", etc.
     * agent_context: "primary", "subagent", "cron", "flush".
     * agent_identity: profile name (e.g. "coder").
     * agent_workspace: shared workspace name.
     * parent_session_id: for subagents, parent's session_id.
     * user_id: platform user identifier.
     * user_id_alt: optional alternate stable user identifier. */
    void (*initialize)(memory_provider_t *self, const char *session_id,
                       const char *hermes_home, const char *platform,
                       const char *agent_context, const char *agent_identity,
                       const char *agent_workspace,
                       const char *parent_session_id,
                       const char *user_id, const char *user_id_alt);

    /* Return text to include in system prompt (static info).
     * Return empty string to skip. */
    char *(*system_prompt_block)(memory_provider_t *self);

    /* Recall relevant context for upcoming turn.
     * Return formatted text to inject, or empty string.
     * Should be fast — use background threads for actual recall. */
    char *(*prefetch)(memory_provider_t *self, const char *query,
                      const char *session_id);

    /* Queue background recall for NEXT turn. Default no-op. */
    void (*queue_prefetch)(memory_provider_t *self, const char *query,
                           const char *session_id);

    /* Persist a completed turn. Should be non-blocking.
     * messages: OpenAI-style conversation message list (JSON string). */
    void (*sync_turn)(memory_provider_t *self,
                      const char *user_content, const char *assistant_content,
                      const char *session_id, const char *messages_json);

    /* Return tool schemas this provider exposes.
     * Each schema: {"name": "...", "description": "...", "parameters": {...}}
     * Return empty list if context-only. */
    json_node_t *(*get_tool_schemas)(memory_provider_t *self);

    /* Handle a tool call. Must return JSON string (tool result).
     * Only called for tool names from get_tool_schemas(). */
    char *(*handle_tool_call)(memory_provider_t *self, const char *tool_name,
                              json_node_t *args);

    /* Check if this provider handles a specific tool name.
     * Returns true if any registered tool matches the name.
     * Port of Python memory_manager.has_tool(). */
    bool (*has_tool)(memory_provider_t *self, const char *tool_name);

    /* Clean shutdown — flush queues, close connections. */
    void (*shutdown)(memory_provider_t *self);

    /* -- Optional hooks (default no-op / empty) --------------------- */

    /* Called at start of each turn. kwargs: remaining_tokens, model, etc. */
    void (*on_turn_start)(memory_provider_t *self, int turn_number,
                          const char *message);

    /* Called when session ends. messages: full conversation history (JSON). */
    void (*on_session_end)(memory_provider_t *self, const char *messages_json);

    /* Called when agent switches session_id mid-process.
     * new_session_id: the session_id switched to.
     * parent_session_id: previous session_id (for /branch, /resume).
     * reset: true for /reset / /new (flush buffers).
     * rewound: true if transcript truncated but session_id unchanged. */
    void (*on_session_switch)(memory_provider_t *self,
                              const char *new_session_id,
                              const char *parent_session_id,
                              bool reset, bool rewound);

    /* Called before context compression. Return text for compression prompt. */
    char *(*on_pre_compress)(memory_provider_t *self, const char *messages_json);

    /* Called on parent when subagent completes.
     * task: delegation prompt. result: subagent's final response. */
    void (*on_delegation)(memory_provider_t *self, const char *task,
                          const char *result, const char *child_session_id);

    /* Return config fields for 'hermes memory setup'.
     * Returns NULL-terminated array of config field descriptors. */
    memory_provider_config_field_t *(*get_config_schema)(memory_provider_t *self);

    /* Write non-secret config to provider's native location.
     * values: JSON string of non-secret fields.
     * hermes_home: active HERMES_HOME path. */
    void (*save_config)(memory_provider_t *self, const char *values_json,
                        const char *hermes_home);

    /* Called when built-in memory tool writes an entry.
     * action: "add", "replace", "remove".
     * target: "memory" or "user".
     * content: entry content.
     * metadata: JSON string with provenance (write_origin, session_id, etc.). */
    void (*on_memory_write)(memory_provider_t *self, const char *action,
                            const char *target, const char *content,
                            const char *metadata_json);
};

/* ================================================================
 *  Memory Provider Base Struct
 * ================================================================ */

struct memory_provider_t {
    const memory_provider_vtable_t *vtable;
    void            *plugin_handle;     /* Plugin .so handle (dlopen) */
    char            *session_id;
    char            *hermes_home;
    char            *platform;
    bool             initialized;
    /* Provider-specific data follows in plugin-allocated memory */
};

/* ================================================================
 *  Default no-op implementations
 * ================================================================ */

/* These are used as defaults when a provider doesn't override an optional hook. */

static inline void memory_provider_noop_turn_start(memory_provider_t *self,
                                                    int turn_number,
                                                    const char *message) {
    (void)self; (void)turn_number; (void)message;
}

static inline void memory_provider_noop_session_end(memory_provider_t *self,
                                                     const char *messages_json) {
    (void)self; (void)messages_json;
}

static inline void memory_provider_noop_session_switch(memory_provider_t *self,
                                                        const char *new_sid,
                                                        const char *parent_sid,
                                                        bool reset, bool rewound) {
    (void)self; (void)new_sid; (void)parent_sid; (void)reset; (void)rewound;
}

static inline char *memory_provider_empty_pre_compress(memory_provider_t *self,
                                                         const char *messages_json) {
    (void)self; (void)messages_json;
    char *s = malloc(1);
    if (s) s[0] = '\0';
    return s;
}

static inline void memory_provider_noop_queue_prefetch(memory_provider_t *self,
                                                        const char *query,
                                                        const char *session_id) {
    (void)self; (void)query; (void)session_id;
}

static inline void memory_provider_noop_sync_turn(memory_provider_t *self,
                                                   const char *user,
                                                   const char *asst,
                                                   const char *session_id,
                                                   const char *msgs) {
    (void)self; (void)user; (void)asst; (void)session_id; (void)msgs;
}

static inline void memory_provider_noop_shutdown(memory_provider_t *self) {
    (void)self;
}

static inline void memory_provider_noop_delegation(memory_provider_t *self,
                                                    const char *task,
                                                    const char *result,
                                                    const char *child_sid) {
    (void)self; (void)task; (void)result; (void)child_sid;
}

static inline void memory_provider_noop_save_config(memory_provider_t *self,
                                                     const char *values,
                                                     const char *home) {
    (void)self; (void)values; (void)home;
}

static inline void memory_provider_noop_memory_write(memory_provider_t *self,
                                                      const char *action,
                                                      const char *target,
                                                      const char *content,
                                                      const char *metadata) {
    (void)self; (void)action; (void)target; (void)content; (void)metadata;
}

/* Default noop has_tool — returns false (no tools registered).
 * Port of Python memory_manager.has_tool(). */
static inline bool memory_provider_noop_has_tool(memory_provider_t *self,
                                                  const char *tool_name) {
    (void)self; (void)tool_name;
    return false;
}

/* ================================================================
 *  Built-in (default) memory provider
 * ================================================================ */

/* The built-in provider uses the local memory store (memory.c).
 * It exposes no extra tools and does no external sync. */

memory_provider_t *memory_provider_builtin_create(void);
void memory_provider_builtin_destroy(memory_provider_t *p);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_PROVIDER_H */
