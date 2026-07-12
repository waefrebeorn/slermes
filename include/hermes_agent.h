/**
 * @defgroup hermes_agent Agent Loop
 * @brief Core conversation loop and state management.
 *
 *
Defines agent_state_t, agent_run_conversation(), and the main
LLM interaction loop with tool calling, session persistence,
streaming, and interrupt handling.
 *
 * @{
 */
#ifndef HERMES_AGENT_H
#define HERMES_AGENT_H

/*
 * hermes_agent.h — Agent-related declarations for Hermes C.
 * Extends the types in hermes.h with agent-specific functions.
 */

#include "hermes_core_types.h"

/* session_meta_t is a complete type defined in lib/libdb/db.h
 * (anonymous-struct typedef, cannot be forward-declared by tag).
 * session_entry_t is also provided by lib/libdb/db.h (forward-declared
 * there). Pulling in db.h keeps hermes_agent.h self-contained — no
 * god-header pull-in. */
#include "libdb/db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === Tool Registry (tools/registry.c) === */
bool registry_register(const char *name, const char *description,
                        const char *schema_json,
                        char *(*handler)(const char *args_json, const char *task_id));
/* P150: Extended registration with toolset name for enabled/disabled filtering */
bool registry_register_ex(const char *name, const char *description,
                          const char *schema_json, const char *toolset,
                          char *(*handler)(const char *args_json, const char *task_id));
void registry_set_available(const char *name, bool available);
tool_t *registry_find(const char *name);
char *registry_repair_tool_name(const char *tool_name);
char *registry_dispatch(const char *name, const char *args_json,
                        const char *task_id);
tool_registry_t *get_registry(void);
size_t registry_count(void);
json_node_t *registry_to_json(void);

/* Tool initialization */
void tools_init_all(void);

/* === Message lifecycle (context.c) === */
message_t *message_new(message_role_t role, const char *content);
message_t *message_new_tool(const char *tool_call_id, const char *content);
message_t *message_new_assistant(const char *content, const char *tool_name,
                                  const char *tool_call_id, const char *reasoning,
                                  const char *encrypted_content);
message_t *message_new_assistant_with_toolcalls(const char *content,
                                                  const tool_call_t *tcalls,
                                                  int tcalls_count,
                                                  const char *reasoning,
                                                  const char *encrypted_content);
void message_free(message_t *msg);
/* Clone a message (deep copy) */
message_t *message_clone(const message_t *src);

/* === Context operations (context.c) === */
void context_init(agent_state_t *state);
bool context_push(agent_state_t *state, message_t *msg);
message_t *context_pop(agent_state_t *state);
void context_clear(agent_state_t *state);
bool context_set_system(agent_state_t *state, const char *content);
void context_truncate(agent_state_t *state, size_t max_messages);
const message_t *context_get(const agent_state_t *state, size_t index);

/* P90: Smart context eviction strategies */
typedef enum {
    EVICT_OLDEST_TOOL_FIRST,
    EVICT_OLDEST_USER,
    EVICT_KEEP_RECENT_N,
} eviction_strategy_t;

int  context_message_tokens(const message_t *msg);
int  context_total_tokens(const agent_state_t *state);
void context_evict_smart(agent_state_t *state, size_t max_messages,
                          eviction_strategy_t strategy);
const char *context_get_system(const agent_state_t *state);

/* P97: Compression feedback — user-rated quality and adaptive threshold */
void compression_feedback_init(compression_feedback_t *fb);
void compression_feedback_positive(compression_feedback_t *fb);
void compression_feedback_negative(compression_feedback_t *fb);
float compression_feedback_get_threshold(const compression_feedback_t *fb, float config_threshold);
void compression_feedback_status(const compression_feedback_t *fb, char *buf, size_t sz);

/* P98: Checkpoint manager — auto-save, named checkpoints, rollback */
void    checkpoint_init(checkpoint_manager_t *mgr);
void    checkpoint_free(checkpoint_manager_t *mgr);
void    checkpoint_set_limits(checkpoint_manager_t *mgr, int max_snapshots, int auto_save_interval);
bool    checkpoint_save(checkpoint_manager_t *mgr, agent_state_t *state, const char *label);
bool    checkpoint_restore(checkpoint_manager_t *mgr, agent_state_t *state, const char *checkpoint_id);
size_t  checkpoint_list(const checkpoint_manager_t *mgr, char (*ids)[64], char (*labels)[128], size_t max_count);
size_t  checkpoint_count(const checkpoint_manager_t *mgr);
bool    checkpoint_try_autosave(checkpoint_manager_t *mgr, agent_state_t *state);

/* G29: Generate diff between checkpoint and current state. Caller must free. */
char    *checkpoint_diff(const checkpoint_t *cp, const agent_state_t *state);

/* G30: Restore checkpoint and branch from that point. */
bool    checkpoint_branch_restore(checkpoint_manager_t *mgr, agent_state_t *state,
                                  const char *checkpoint_id, const char *new_session_id);

/* v322: Filesystem persistence */
bool    checkpoint_init_dir(void);
bool    checkpoint_persist_save(const checkpoint_t *cp);
checkpoint_t *checkpoint_persist_load(const char *checkpoint_id);
size_t  checkpoint_persist_list(char (*ids)[64], size_t max_count);
size_t  checkpoint_persist_prune(int retention_days);
/* Port of Python: maybe_auto_prune_checkpoints */
bool    checkpoint_maybe_auto_prune(int retention_days, int min_interval_hours);
/* Port of Python: _dir_file_count, _dir_size_bytes, clear_all */
size_t  checkpoint_dir_file_count(void);
size_t  checkpoint_dir_size_bytes(void);
size_t  checkpoint_clear_all(void);
/* Port of Python: format_checkpoint_list */
char   *checkpoint_format_list(size_t count, const char (*ids)[64], const char (*labels)[128]);
/* Port of Python: clear_legacy */
size_t  checkpoint_clear_legacy(void);

/* === LLM Client (llm_client.c) === */
llm_response_t *llm_chat_completion(llm_config_t *cfg,
                                     const message_t **messages,
                                     size_t message_count,
                                     json_node_t *tools_json);

/* Streaming variant. Calls token_cb with each content delta as it arrives.
 * Still returns full llm_response_t with accumulated content + tool_calls. */
llm_response_t *llm_chat_completion_stream(llm_config_t *cfg,
                                            const message_t **messages,
                                            size_t message_count,
                                            json_node_t *tools_json,
                                            llm_token_cb_t token_cb,
                                            void *userdata);
void llm_response_free(llm_response_t *resp);

/* Token estimation (approximate: 1 token ≈ 4 chars) */
static inline size_t llm_estimate_tokens(const char *text) {
    if (!text) return 0;
    return (strlen(text) + 3) / 4; /* ceil division */
}

/* Count approximate tokens in a message list */
size_t llm_count_context_tokens(const message_t **msgs, size_t count, size_t max_tokens);

/* Truncate context to fit within token budget. Keeps first (system) and last N. */
void llm_truncate_context(agent_state_t *state, size_t max_tokens);

/* Smart context compression: summarize old messages before dropping.
 * Returns summary string (caller inserts) or NULL (fall back to dropping).
 * Pass compression.enabled flag from config. */
char *llm_compress_context(agent_state_t *state, size_t max_tokens, bool enabled);

/* P100: Background review — lightweight AI review of tool results.
 * Returns malloc'd review text, caller must free. */
char *llm_background_review(llm_config_t *cfg,
                             const char *tool_name,
                             const char *tool_args,
                             const char *tool_result);

/* Port of Python agent/background_review.py:summarize_background_review_actions().
 * Walk review messages (JSON arrays), filter out ones in prior_snapshot,
 * and return a JSON array of action descriptions. Caller must free. */
char *summarize_background_review_actions(const char *review_messages_json,
                                           const char *prior_snapshot_json);

/* === Agent Loop (agent_loop.c) === */
void init_agent(agent_state_t *state);
void agent_generate_session_id(agent_state_t *state);
void agent_free(agent_state_t *state);
void agent_configure_from_config(agent_state_t *state, const hermes_config_t *cfg);
char *agent_run_conversation(agent_state_t *state,
                              const char *user_message,
                              const char *system_message);
char *run_conversation(agent_state_t *state,
                        const char *user_message,
                        const char *system_message);
char *agent_chat(agent_state_t *state, const char *message);

/* G18: Inject conversation history — preload messages from JSON array */
bool agent_inject_history(agent_state_t *state, const char *history_json);

/* Serialize/deserialize messages for session persistence */
char *serialize_messages(const agent_state_t *state);

/* === Session persistence (agent_loop.c) === */
bool agent_open_db(agent_state_t *state);
bool agent_save_session(agent_state_t *state);
bool agent_load_session(agent_state_t *state, const char *session_id);
void agent_close_db(agent_state_t *state);

/* P141-P150: Session DB depth */
bool agent_save_meta(agent_state_t *state);
bool agent_load_meta(agent_state_t *state, session_meta_t *meta);
bool agent_session_create(agent_state_t *state, const char *session_id);
session_entry_t *agent_session_list(size_t *count, const char *tag_filter, int limit);
bool agent_session_delete(agent_state_t *state, const char *session_id);
bool agent_auto_save(agent_state_t *state, int interval);
bool agent_crash_recover(agent_state_t *state);
int  agent_auto_prune(agent_state_t *state, int retention_days);
bool agent_session_add_tag(agent_state_t *state, const char *tag);
bool agent_session_remove_tag(agent_state_t *state, const char *tag);
char *agent_session_export_json(agent_state_t *state, const char *session_id);
char *agent_session_export_markdown(agent_state_t *state, const char *session_id);
bool agent_session_branch(agent_state_t *state, const char *new_id, int branch_point);
int  agent_session_migrate(agent_state_t *state);

/* P28: Undo snapshot — capture/restore message state */
void agent_snapshot_take(agent_state_t *state);
bool agent_snapshot_restore(agent_state_t *state);

/* === Turn Finalization (turn_finalizer.c) — Port of Python agent/turn_finalizer.py === */
/* Post-loop turn finalization: budget exhaustion summary, turn-exit diagnostics,
 * interrupt/budget explainer messages, trajectory save, result return. */
char *finalize_turn(agent_state_t *state);

/* === Title (title.c) — Port of Python agent/title_generator.py === */
/* Port of Python: generate_title() */
char *agent_generate_title(llm_config_t *cfg, const char *first_message);
/* Port of Python: auto_title_session() */
void auto_title_session(const char *session_id, const char *user_message,
                        const char *assistant_response);
/* Port of Python: maybe_auto_title() */
void maybe_auto_title(const char *session_id, const char *user_message,
                      const char *assistant_response,
                      int user_message_count);

/* === Stream diagnostics (stream_diag.c) === */
void stream_diag_capture_response(stream_diag_t *diag);
char *flatten_error_chain(const hermes_error_t *err);
void log_stream_retry(
    const char *provider,
    const char *base_url,
    const hermes_error_t *error,
    int attempt,
    int max_attempts,
    bool mid_tool_call,
    const stream_diag_t *diag);
void emit_stream_drop(
    const char *provider,
    const char *base_url,
    const hermes_error_t *error,
    int attempt,
    int max_attempts,
    bool mid_tool_call,
    const stream_diag_t *diag);

/* === Chat completion helpers (chat_completion_helpers.c) === */
int estimate_request_context_tokens(const message_t **messages, size_t message_count,
                                    const json_node_t *tools_json);
json_node_t *build_api_kwargs(agent_state_t *state, const message_t **messages,
                               size_t message_count, const json_node_t *tools_json);
message_t *build_assistant_message(const llm_response_t *resp);
bool handle_max_iterations(agent_state_t *state);
bool try_activate_fallback(agent_state_t *state);
void cleanup_task_resources(const char *task_id);

/* Port of Python chat_completion_helpers._is_openai_codex_backend(). */
bool is_openai_codex_backend(const char *api_mode);

/* Port of Python chat_completion_helpers._env_float(). */
double env_float(const char *name, double default_val);
int repair_message_sequence(message_t **messages, int *count);
int sanitize_tool_call_arguments(message_t *messages, int *count);

/* Port of Python agent_runtime_helpers.agent_runtime_owns_post_tool_hook().
 * Check if a tool path emits its own post hook. */
bool agent_runtime_owns_post_tool_hook(const char *function_name);

/* Port of Python agent_runtime_helpers.drop_thinking_only_and_merge_users().
 * Drop thinking-only assistant messages, merging adjacent user messages. */
int drop_thinking_only_and_merge_users(message_t **messages, int *count);

/* Port of Python agent_runtime_helpers.apply_pending_steer_to_tool_results().
 * Append pending /steer text to the last tool result in the recent tail.
 * Drains pending_steer, finds the last MSG_TOOL, appends with marker.
 * Returns chars appended, or 0 if nothing was done. */
int apply_pending_steer_to_tool_results(message_t **messages, int *count,
                                         int num_tool_msgs, char *pending_steer);

/* Port of Python agent/agent_runtime_helpers.py:restore_primary_runtime().
 * Restore the primary runtime at the start of a new turn.
 * Returns true if primary runtime was restored, false otherwise. */
bool restore_primary_runtime(agent_state_t *agent);

/* === Message sanitization (agent_message_sanitize.c) ===
 * Post-response sanitization pipeline: surrogate fix, think-block
 * stripping, secret redaction. Port of build_assistant_message() internals. */
bool hermes_message_sanitize(message_t *msg);

/* Port of Python agent/agent_runtime_helpers.py:looks_like_codex_intermediate_ack().
 * Detect a planning/ack message that should continue instead of ending the turn. */
bool looks_like_codex_intermediate_ack(const message_t *const *messages,
                                        int msg_count,
                                        const char *user_message,
                                        const char *assistant_content);

/* Port of Python agent/agent_runtime_helpers.py:extract_reasoning().
 * Extract reasoning/thinking content from an assistant message.
 * Returns malloc'd string or NULL. Caller must free(). */
char *extract_reasoning(const char *content, const char *reasoning);

/* Port of Python run_agent.py AIAgent._needs_thinking_reasoning_pad().
 * Return true when the active provider enforces reasoning_content echo-back
 * (DeepSeek V4, Kimi/Moonshot, Xiaomi MiMo thinking modes). */
bool needs_thinking_reasoning_pad(const char *provider,
                                   const char *base_url,
                                   const char *model);

/* Port of Python agent/agent_runtime_helpers.py:reapply_reasoning_echo_for_provider().
 * Re-pads assistant turns with reasoning_content for providers that require it.
 * Operates on a JSON array of API message objects. Returns padded count. */
int reapply_reasoning_echo_for_provider(json_node_t *api_messages,
                                         const char *provider,
                                         const char *base_url,
                                         const char *model);

/* Port of Python agent/agent_runtime_helpers.py:copy_reasoning_content_for_api().
 * Copy provider-facing reasoning fields from a stored message onto an API
 * replay message. Handles edge cases for DeepSeek/Kimi/MiMo thinking modes. */
void copy_reasoning_content_for_api(json_node_t *source_msg,
                                     json_node_t *api_msg,
                                     const char *provider,
                                     const char *base_url,
                                     const char *model);

/* Port of Python agent/agent_runtime_helpers.py:dump_api_request_debug().
 * Dump a debug-friendly HTTP request record for the active inference API.
 * Captures the request body and error context. Intended for debugging
 * provider-side 4xx failures where retries are not useful.
 * Returns true if the dump file was written successfully. */
bool dump_api_request_debug(const char *request_body, const char *session_id,
                             const char *reason, const char *provider,
                             const char *model, const char *url,
                             int http_status, const char *error_body);

/* Port of Python agent/agent_runtime_helpers.py:try_recover_primary_transport().
 * Give the primary provider one more attempt after retries are exhausted,
 * using a fresh connection. Skip for aggregator providers.
 * Returns malloc'd response or NULL. Caller frees. */
llm_response_t *try_recover_primary_transport(llm_config_t *cfg,
    const message_t **messages, size_t message_count,
    json_node_t *tools_json,
    llm_token_cb_t stream_cb, void *stream_data);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_agent group */

/**
 * Wrap an inner Gemini request in the Code Assist API envelope (port of
 * gemini_cloudcode_adapter.py:wrap_code_assist_request).
 * Returns malloc'd JSON string (caller must free), or NULL on OOM.
 */
char *wrap_code_assist_request(const char *project_id, const char *model,
                                const char *inner_request_json,
                                const char *user_prompt_id);

#endif /* HERMES_AGENT_H */
