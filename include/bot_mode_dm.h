/*
 * bot_mode_dm.h — C11 port of tools/bot_mode_dm.py.
 *
 * Bot Mode agent-to-agent DM tool — ``message_agent``. Structured,
 * Bot-Chat-only tool that lets a Bot Mode agent message a teammate agent
 * without hand-assembling shell commands. Fire-and-forget delivery through
 * the background terminal spawn (injectable vtab seam in C).
 */
#ifndef SLERMES_BOT_MODE_DM_H
#define SLERMES_BOT_MODE_DM_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESSAGE_AGENT_TOOL_NAME "message_agent"
#define MESSAGE_MAX_CHARS 16000
#define BOT_MODE_DM_BOT_CHAT_TITLE "Bot Chat"

/* Agent-context seam (C vtab for Python's `agent` duck-typed object). */
typedef struct {
    void *ctx;
    /* _bot_mode_protocol flag (Python getattr(agent, "_bot_mode_protocol",
     * True)); return nonzero when enabled. */
    int (*protocol_enabled)(void *ctx);
    /* Live tools list (JSON array) or NULL when absent. */
    json_t *(*get_tools)(void *ctx);
    /* Append the schema to the agent's tool list (creates it when NULL). */
    int (*append_tool)(void *ctx, const json_t *schema);
    /* valid_tool_names set add (no-op when the agent has no set). */
    void (*add_valid_tool_name)(void *ctx, const char *name);
    /* _session_title: hint, else session-db lookup; malloc'd or NULL. */
    char *(*session_title)(void *ctx);
    /* _agent_home: the calling agent's OWN home; malloc'd or NULL. */
    char *(*agent_home)(void *ctx);
} bot_mode_dm_agent_vtab_t;

/* Background terminal spawn seam (terminal_tool(background=True,
 * notify_on_complete=True, task_id=...)). Returns the raw terminal_tool
 * JSON output (malloc'd) or NULL on exception with *err set. */
typedef char *(*dm_terminal_spawn_cb)(void *ctx, const char *command,
                                      const char *task_id, char **err);
void bot_mode_dm_set_spawn(dm_terminal_spawn_cb cb, void *ctx);

/* message_agent_tool_schema @ tools/bot_mode_dm.py:message_agent_tool_schema
 * Returns a NEW schema object (caller frees); NULL on alloc failure. */
json_t *message_agent_tool_schema(void);

/* ensure_message_agent_tool @ tools/bot_mode_dm.py:ensure_message_agent_tool
 * Inject the schema into a Bot Chat agent's tool list. Idempotent. */
int ensure_message_agent_tool(const bot_mode_dm_agent_vtab_t *agent);

/* message_agent_tool @ tools/bot_mode_dm.py:message_agent_tool
 * Deliver `message` to `target`'s Bot Chat. Returns malloc'd JSON
 * ack/error string (caller frees). */
char *message_agent_tool(const char *target, const char *message,
                         const char *task_id,
                         const bot_mode_dm_agent_vtab_t *agent);

/* Roster helpers (exposed for the oracle twin). */
char *bot_mode_dm_hermes_root(const char *home, char *out, size_t n);   /* _hermes_root */
char *bot_mode_dm_self_profile_name(const char *home, char *out, size_t n); /* _self_profile_name */
char **bot_mode_dm_local_roster(const char *root, size_t *count);       /* _local_roster */
char *bot_mode_dm_handle(const char *name, char *out, size_t n);        /* _handle */
char *bot_mode_dm_resolve_local_name(const char *target,
                                     const char *const *roster, size_t n); /* _resolve_local_name; malloc'd or NULL */
char *bot_mode_dm_write_dm_file(const char *content);                   /* _write_dm_file; malloc'd path */

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BOT_MODE_DM_H */
