/**
 * @file hermes_gateway_yuanbao.h
 * @brief Yuanbao WebSocket gateway platform declarations.
 *
 * Extracted from hermes_gateway.h god header.
 */
#ifndef HERMES_GATEWAY_YUANBAO_H
#define HERMES_GATEWAY_YUANBAO_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  yuanbao — Yuanbao WebSocket gateway
 * ================================================================ */

/* ---- Basic config & lifecycle ---- */
bool yuanbao_init(const char *app_id, const char *app_secret,
                  const char *bot_id, const char *ws_url,
                  const char *api_domain);
void yuanbao_start(void);
void yuanbao_stop(void);
/* Returns true while the yuanbao WebSocket supervisor loop is running. */
bool yuanbao_is_running(void);

/* ---- Sticker / messaging ---- */
int  yuanbao_send_sticker(const char *to_uid, const char *sticker_id,
                           const char *sticker_name, const char *package_id,
                           int width, int height);
char *yuanbao_query_group_info(const char *group_code, int timeout_sec);
char *yuanbao_get_group_member_list(const char *group_code, uint32_t offset, uint32_t limit, int timeout_sec);
char *yuanbao_send_dm(const char *to_uid, const char *text);

/* ================================================================
 *  SignManager — Token acquisition & caching (port of gateway/platforms/yuanbao.py:SignManager)
 * ================================================================ */

/* Opaque token cache entry */
typedef struct {
    char token[512];
    char bot_id[128];
    int  duration;
    char product[64];
    char source[64];
    double expire_ts;   /* Unix epoch when token expires */
} yuanbao_token_entry_t;

/* Get a per-app_key refresh lock (creates if not exists).
 * Must be called from a thread with pthread support.
 * Returns a mutex pointer that the caller must lock/unlock.
 * The lock is stored in a static map and survives until process exit. */
pthread_mutex_t *yuanbao_sign_get_refresh_lock(const char *app_key);

/* Compute HMAC-SHA256 signature per Yuanbao spec.
 * plain = nonce + timestamp + app_key + app_secret
 * signature = HMAC-SHA256(key=app_secret, msg=plain).hexdigest()
 * Returns malloc'd string (caller must free). NULL on error. */
char *yuanbao_sign_compute_signature(const char *nonce, const char *timestamp,
                                      const char *app_key, const char *app_secret);

/* Build Beijing-time ISO-8601 timestamp without milliseconds.
 * Format: "2006-01-02T15:04:05+08:00"
 * Returns malloc'd string (caller must free). NULL on error. */
char *yuanbao_sign_build_timestamp(void);

/* Check if a cached token entry is still valid (with refresh margin).
 * Returns true if (entry.expire_ts - now) > CACHE_REFRESH_MARGIN_S (60s). */
bool yuanbao_sign_is_cache_valid(const yuanbao_token_entry_t *entry);

/* Clear all refresh locks (called on disconnect). */
void yuanbao_sign_clear_locks(void);

/* Remove all expired entries from the token cache.
 * Returns number of entries purged. */
int yuanbao_sign_purge_expired(void);

/* Fetch a fresh token from the sign-token HTTP endpoint.
 * Uses the provided http_client_t (or creates one if NULL).
 * Returns malloc'd JSON string with token data (caller must free).
 * Format: {"token":"...","bot_id":"...","duration":...,"product":"...","source":"..."}
 * Returns NULL on failure. */
char *yuanbao_sign_fetch(http_client_t *http,
                          const char *app_key, const char *app_secret,
                          const char *api_domain, const char *route_env);

/* Get token from cache or fetch/refresh if needed (single-flight per app_key).
 * Returns malloc'd JSON string (same format as fetch). Caller must free.
 * Returns NULL on failure. */
char *yuanbao_sign_get_token(http_client_t *http,
                              const char *app_key, const char *app_secret,
                              const char *api_domain, const char *route_env);

/* Force refresh token (clear cache and re-fetch).
 * Returns malloc'd JSON string (same format). Caller must free. */
char *yuanbao_sign_force_refresh(http_client_t *http,
                                  const char *app_key, const char *app_secret,
                                  const char *api_domain, const char *route_env);

/* ================================================================
 *  MarkdownProcessor — Streaming markdown utilities (port of gateway/platforms/yuanbao.py:MarkdownProcessor)
 * ================================================================ */

/* Detect unclosed code block fences in text. */
bool yuanbao_md_has_unclosed_fence(const char *text);

/* Detect if text ends with a table row (|...|). */
bool yuanbao_md_ends_with_table_row(const char *text);

/* Split text at paragraph boundary near max_len.
 * Returns malloc'd string with first part (caller must free).
 * The remainder is set in *tail_out (caller must free). */
char *yuanbao_md_split_at_paragraph_boundary(const char *text, size_t max_len, char **tail_out);

/* Detect if text is a fenced-code-block atom (starts and ends with ```). */
bool yuanbao_md_is_fence_atom(const char *text);

/* Detect if text is a table atom (starts and ends with |...|). */
bool yuanbao_md_is_table_atom(const char *text);

/* Split text into atoms (fences, tables, paragraphs separated by blank lines).
 * Returns NULL-terminated array of malloc'd strings. Caller frees each + array. */
char **yuanbao_md_split_into_atoms(const char *text);

/* Chunk markdown text for streaming (respects fence/table boundaries).
 * Returns NULL-terminated array of malloc'd chunks. Caller frees each + array. */
char **yuanbao_md_chunk_markdown_text(const char *text, size_t max_chunk_size);

/* Infer separator between two chunks: "\n" for fence/table continuation, else "\n\n".
 * Returns malloc'd string (caller must free). */
char *yuanbao_md_infer_block_separator(const char *prev_chunk, const char *next_chunk);

/* Merge adjacent chunks that share streaming fences (```).
 * Input: NULL-terminated array of malloc'd strings.
 * Returns new NULL-terminated array. Caller frees input array (not strings) + output. */
char **yuanbao_md_merge_block_streaming_fences(char **chunks);

/* Strip outer markdown fence if present (```lang\ncontent\n``` -> content).
 * Returns malloc'd string (caller must free). */
char *yuanbao_md_strip_outer_markdown_fence(const char *text);

/* Sanitize markdown tables (ensure consistent pipe alignment).
 * Returns malloc'd string (caller must free). */
char *yuanbao_md_sanitize_markdown_table(const char *text);

/* Build system prompt hint for markdown formatting.
 * Returns static const string (no free needed). */
const char *yuanbao_md_markdown_hint_system_prompt(void);

/* Regex pattern for Yuanbao resource references: [kind|ybres:id]
 * Caller provides output buffers (must be at least 128 bytes each).
 * Returns true if a match was found, filling out_kind and out_id. */
bool yuanbao_md_extract_resource_ref(const char *text, char *out_kind, char *out_id);

/* Regex pattern for local media anchors: [kind: /path/to/file]
 * Returns malloc'd string with the path, or NULL. */
char *yuanbao_md_extract_local_media_path(const char *text);

/* Strip page indicators like "(1/3)" from end of text.
 * Returns malloc'd string (caller must free). */
char *yuanbao_md_strip_page_indicator(const char *text);

/* ================================================================
 *  Constants
 * ================================================================ */

#define YUANBAO_HEARTBEAT_INTERVAL_S       30.0
#define YUANBAO_CONNECT_TIMEOUT_S          15.0
#define YUANBAO_AUTH_TIMEOUT_S             10.0
#define YUANBAO_MAX_RECONNECT_ATTEMPTS     100
#define YUANBAO_DEFAULT_SEND_TIMEOUT       30.0
#define YUANBAO_WS_CLOSE_TIMEOUT_S         1.0
#define YUANBAO_HEARTBEAT_TIMEOUT_THRESHOLD 2
#define YUANBAO_REPLY_HEARTBEAT_INTERVAL_S 2.0
#define YUANBAO_REPLY_HEARTBEAT_TIMEOUT_S  30.0
#define YUANBAO_REPLY_REF_TTL_S            300.0
#define YUANBAO_SLOW_RESPONSE_TIMEOUT_S    120.0
#define YUANBAO_OBSERVED_MEDIA_BACKFILL_LOOKBACK 50
#define YUANBAO_OBSERVED_MEDIA_BACKFILL_MAX_RESOLVE 12
#define YUANBAO_SIGN_MAX_RETRIES           3
#define YUANBAO_SIGN_RETRY_DELAY_S         2.0
#define YUANBAO_SIGN_HTTP_TIMEOUT_S        10.0
#define YUANBAO_SIGN_CACHE_REFRESH_MARGIN_S 60.0
#define YUANBAO_SIGN_TOKEN_PATH            "/api/bot/sign_token"
#define YUANBAO_SIGN_RETRYABLE_CODE        10003
#define YUANBAO_SIGN_APP_VERSION           "1.0.0"
#define YUANBAO_SIGN_OPERATION_SYSTEM      "linux"
#define YUANBAO_SIGN_BOT_VERSION           "1.0.0"
#define YUANBAO_SIGN_INSTANCE_ID           1234567890123456789ULL

/** @} */
#endif /* HERMES_GATEWAY_YUANBAO_H */