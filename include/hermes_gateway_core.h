/**
 * @file hermes_gateway_core.h
 * @brief Core gateway API declarations (session, queue, rate limiter, pool, platform registry, hooks, events, formatting, error handling).
 */
#ifndef HERMES_GATEWAY_CORE_H
#define HERMES_GATEWAY_CORE_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"
#include "hermes_json.h"

/* ================================================================
 *  Session source helpers
 * ================================================================ */

int session_source_description(const gw_session_source_t *src,
                                char *buf, size_t sz);

void session_source_set(gw_session_source_t *src,
                         const char *platform,
                         const char *chat_id,
                         const char *chat_name,
                         const char *chat_type,
                         const char *user_id,
                         const char *user_name,
                         const char *thread_id,
                         const char *chat_topic,
                         const char *user_id_alt,
                         const char *chat_id_alt,
                         const char *guild_id,
                         const char *parent_chat_id,
                         const char *message_id,
                         bool is_bot);

bool gw_session_set_source(const char *platform, const char *chat_id,
                            const gw_session_source_t *source);

gw_session_source_t *gw_session_get_source(const char *platform, const char *chat_id);

char *build_session_context_prompt(const gw_session_source_t *src);

/* Thread-safe agent_chat wrapper */
char *gateway_agent_chat(const char *message);

/* ================================================================
 *  E28: Deduplication
 * ================================================================ */

bool gw_dedup_check(const char *message_id);
void gw_dedup_add(const char *message_id);

/* ================================================================
 *  E29: Batch aggregation
 * ================================================================ */

void gw_batch_accumulate(const char *platform, const char *chat_id, const char *fragment);
void gw_batch_flush(void);

/* ================================================================
 *  E31: Per-platform cooldown
 * ================================================================ */

double gw_cooldown_remaining(int plat_idx);
void gw_cooldown_mark(int plat_idx);

/* ================================================================
 *  E32: Reconnect backoff
 * ================================================================ */

double gw_reconnect_delay(int plat_idx);
void gw_reconnect_reset(int plat_idx);

/* ================================================================
 *  E34: Group observe
 * ================================================================ */

void gw_set_group_observe(const char *prefix, bool enabled);
void gw_observe_append(const char *platform, const char *chat_id, const char *text);
char *gw_observe_consume(const char *platform, const char *chat_id);

/* ================================================================
 *  E35-E38: Gateway hooks system
 * ================================================================ */

typedef json_node_t *(*gw_hook_t)(json_node_t *data, void *userdata);
void gw_register_pre_send(gw_hook_t hook, void *userdata);
void gw_register_post_receive(gw_hook_t hook, void *userdata);
void gw_register_interceptor(gw_hook_t hook, void *userdata);

/* E38: Gateway event bus */
typedef void (*gw_event_listener_t)(const char *event_type, json_node_t *data, void *userdata);
void gw_event_register(gw_event_listener_t listener, void *userdata);
void gw_event_emit(const char *event_type, json_node_t *data);

/* ================================================================
 *  E40-E43: Gateway formatting utilities
 * ================================================================ */

char *gw_markdown_to_html(const char *text);
char *gw_markdown_v2_escape(const char *text);
char *gw_truncate_message(const char *text, size_t max_len);

/* E80-E81: UTF-16 length helpers (Telegram API compatibility) */
size_t utf16_len(const char *s);
char *gw_prefix_within_utf16_limit(const char *s, size_t limit);

/* Generic binary search: find largest n such that len_fn(s, n) <= budget.
 * Port of Python gateway/platforms/base.py _custom_unit_to_cp().
 * len_fn receives (string, substring_length) and returns unit count.
 * Returns codepoint offset. 0 on invalid input. */
int custom_unit_to_cp(const char *s, int len, int budget,
                         int (*len_fn)(const char *, int));

/* Port of Python gateway/platforms/base.py _float_env().
 * Reads an environment variable and parses it as a double.
 * Returns default_value on missing, empty, or unparseable input. */
double float_env(const char *name, double default_value);

/* Port of Python hermes_cli/observability/relay_shared_metrics.py _retry_ordinal */
int gw_retry_ordinal(const json_t *event);

/* ================================================================
 *  E44-E47: Gateway error handling
 * ================================================================ */

bool gw_retry_with_backoff(bool (*api_call)(void *ctx), void *ctx, int max_retries, int base_delay_ms);
bool gw_refresh_token(int plat_idx);

#endif /* HERMES_GATEWAY_CORE_H */