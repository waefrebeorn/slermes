/**
 * @file hermes_hooks.h
 * @brief P186: Hook registry — event-driven callback dispatch.
 *
 * Lightweight publish/subscribe for lifecycle events.
 * Shell hooks and plugin callbacks register with the same API.
 * Events are string names matching Python's VALID_HOOKS.
 *
 * Usage:
 *   // Register
 *   void my_cb(const char *event, const char *payload, void *userdata);
 *   hook_register("pre_tool_call", my_cb, NULL);
 *
 *   // Invoke all callbacks for an event
 *   invoke_hook("pre_tool_call", "{\"tool_name\":\"terminal\",...}");
 *
 *   // Cleanup
 *   hook_unregister("pre_tool_call", my_cb);
 */
#ifndef HERMES_HOOKS_H
#define HERMES_HOOKS_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Types ──────────────────────────────────────────────────────── */

/** Maximum hook events tracked. */
#define HOOK_MAX_EVENTS   32

/** Maximum callbacks per event. */
#define HOOK_MAX_CBS      16

/** Maximum event name length. */
#define HOOK_EVENT_NAME_MAX 64

/**
 * Callback signature. Receives the event name and a JSON payload.
 * Returns a malloc'd JSON response string, or NULL for no response.
 * The caller frees the returned string.
 */
typedef char *(*hook_callback_t)(const char *event, const char *payload, void *userdata);

/* ── API ────────────────────────────────────────────────────────── */

/**
 * Register a callback for an event.
 * The callback will be called on every invoke_hook for this event.
 * Returns true on success, false if max callbacks reached.
 */
bool hook_register(const char *event, hook_callback_t cb, void *userdata);

/**
 * Unregister a callback for an event.
 * Both the function pointer and userdata must match.
 * Returns true if found and removed.
 */
bool hook_unregister(const char *event, hook_callback_t cb, void *userdata);

/**
 * Invoke all registered callbacks for an event.
 * Each callback receives the JSON payload string.
 * Results are COLLECTED from callbacks that return non-NULL.
 * Returns a malloc'd JSON array of results, or NULL if no results.
 * Caller must free.
 */
char *invoke_hook(const char *event, const char *payload_json);

/**
 * Return true if any callbacks are registered for event.
 */
bool hook_has_callbacks(const char *event);

/**
 * Return the number of registered events.
 */
int hook_event_count(void);

/**
 * Clear all registrations. Useful for testing or shutdown.
 */
void hook_reset_all(void);

/**
 * Convert a shell-hook callback result into a standardized
 * block/allow decision from stdout JSON.
 *
 * Accepts:
 *   {"decision":"block","reason":"..."}   (Claude-Code style)
 *   {"action":"block","message":"..."}    (Hermes canonical)
 *   {"context":"..."}                     (pre_llm_call style)
 *   Empty or non-matching → allow.
 */
typedef enum {
    HOOK_DECISION_ALLOW,
    HOOK_DECISION_BLOCK,
    HOOK_DECISION_CONTEXT,
} hook_decision_t;

typedef struct {
    hook_decision_t decision;
    char            message[512];
} hook_result_t;

/**
 * Parse a callback result string into a structured decision.
 * Returns the parsed decision; message is populated if block.
 */
hook_result_t hook_parse_result(const char *stdout_json);

#ifdef __cplusplus
}
#endif

/* ── Shell hooks integration ────────────────────────────────── */

/**
 * Parse shell hooks config from a JSON object (the "hooks:" config block).
 * Each key is an event name, value is an array of hook specs.
 * Returns number of parsed specs.
 */
int shell_hooks_parse_json(const json_t *hooks_json);

/**
 * Register all parsed shell hooks on the hook registry.
 * Must be called after shell_hooks_parse_json() and before any invoke_hook().
 * Returns number of registered hooks.
 */
int shell_hooks_register_all(void);

/**
 * Shut down shell hooks and clean up registrations.
 */
void shell_hooks_shutdown(void);

/**
 * Return count of configured shell hooks.
 */
int shell_hooks_count(void);

/**
 * Check if (event, command) pair is in the allowlist.
 */
bool shell_hooks_allowlist_check(const char *event, const char *command);

/**
 * Remove every allowlist entry matching command.
 */
int revoke(const char *command);

/**
 * Get the allowlist file path.
 * Port of Python shell_hooks.py allowlist_path().
 */
void allowlist_path(char *buf, size_t sz);

/* Port of Python agent/shell_hooks.py:allowlist_entry_for().
 * Return the allowlist record for this event+command pair, or NULL.
 * Returns a malloc'd JSON string. Caller must free(). */
char *allowlist_entry_for(const char *event, const char *command);

/* Port of Python agent/shell_hooks.py:load_allowlist().
 * Load the allowlist JSON file. Returns a json_t* or NULL.
 * Caller must json_free(). */
json_t *load_allowlist(void);

/* Port of Python agent/shell_hooks.py:save_allowlist().
 * Serialize and write data to the allowlist JSON file.
 * data must be a json_t* object. Returns true on success. */
bool save_allowlist(const json_t *data);

/* Port of Python agent/shell_hooks.py:run_once().
 * Fire a single shell-hook invocation with a synthetic payload.
 * event: the hook event name (e.g. "pre_tool_call").
 * command: the hook command path.
 * json_args: JSON string of kwargs payload, or NULL for empty.
 * Returns malloc'd result JSON string, or NULL on error. Caller frees. */
char *shell_hooks_run_once(const char *event, const char *command,
                            const char *json_args);

/* Port of Python agent/shell_hooks.py:script_is_executable().
 * Return true iff the shell hook script for command is runnable. */
bool script_is_executable(const char *command);

/* Port of Python agent/shell_hooks.py:script_mtime_iso().
 * Return ISO-8601 mtime of the resolved script path, or NULL.
 * Returns malloc'd string. Caller must free(). */
char *script_mtime_iso(const char *command);

/* Port of Python agent/shell_hooks.py:reset_for_tests().
 * Clear the shell hooks config array. Test-only. */
void reset_for_tests(void);

/* Port of Python agent/shell_hooks.py:iter_configured_hooks().
 * Parse the hooks: block from a config JSON object and return
 * the number of specs parsed. */
int iter_configured_hooks(const json_t *config);

/* Port of Python agent/shell_hooks.py:register_from_config().
 * Parse hooks from config JSON and register on hook registry.
 * Returns number of registered hooks, or 0 if none. */
int register_from_config(const json_t *config);

#endif /* HERMES_HOOKS_H */
