/**
 * api_server_adapter.h — Gateway platform adapter for OpenAI-compatible API Server.
 * Port of Python: gateway/platforms/api_server.py (APIServerAdapter)
 *
 * This adapter runs an HTTP server in C using the slermes API server
 * infrastructure and provides the full gateway platform interface.
 */

#ifndef HERMES_API_SERVER_ADAPTER_H
#define HERMES_API_SERVER_ADAPTER_H

#include "hermes_core_types.h"
#include "hermes_gateway.h"
#include "hermes_gateway_config.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_api_server.h"
#include <stdbool.h>
#include <time.h>
#include <sqlite3.h>
#include <pthread.h>
#include "uuid.h"

/* Forward declarations for types defined in base.h */
typedef struct {
    bool success;
    char *message_id;
    char *error;
    void *raw_response;  /* json_node_t */
    bool retryable;
    char **continuation_message_ids;
    size_t continuation_count;
} gw_send_result_t;

/* ── Constants ────────────────────────────────────────────────────── */

#define API_SERVER_DEFAULT_HOST "127.0.0.1"
#define API_SERVER_DEFAULT_PORT 8642
#define MAX_STORED_RESPONSES 100
#define MAX_REQUEST_BYTES 10000000
#define CHAT_COMPLETIONS_SSE_KEEPALIVE_SECONDS 30.0
#define MAX_NORMALIZED_TEXT_LENGTH 65536
#define MAX_CONTENT_LIST_SIZE 1000
#define MAX_SESSION_HEADER_LEN 256
#define MAX_CONCURRENT_RUNS 10
#define RUN_STREAM_TTL 300
#define RUN_STATUS_TTL 3600

/* ── Forward declarations ─────────────────────────────────────────── */

typedef struct api_server_adapter api_server_adapter_t;
typedef struct response_store response_store_t;
typedef struct idempotency_cache idempotency_cache_t;
typedef struct sse_queue sse_queue_t;
typedef struct sse_writer sse_writer_t;
typedef struct run_status run_status_t;

/* ── Response Store (SQLite-backed LRU for Responses API) ─────────── */

/**
 * Initialize the response store.
 * @param max_size Maximum responses to store (LRU eviction)
 * @param db_path Path to SQLite database, or NULL for default
 * @return Response store handle, or NULL on failure
 */
response_store_t *response_store_new(int max_size, const char *db_path);

/**
 * Free the response store and close database connection.
 */
void response_store_free(response_store_t *store);

/**
 * Get a stored response by ID.
 * @param store Response store handle
 * @param response_id Response ID to retrieve
 * @return JSON string of response data, or NULL if not found (caller must free)
 */
char *response_store_get(response_store_t *store, const char *response_id);

/**
 * Store a response with LRU eviction.
 * @param store Response store handle
 * @param response_id Response ID
 * @param data JSON string of response data
 */
void response_store_put(response_store_t *store, const char *response_id, const char *data);

/**
 * Delete a response from the store.
 * @return true if found and deleted
 */
bool response_store_delete(response_store_t *store, const char *response_id);

/**
 * Get conversation's latest response ID.
 */
char *response_store_get_conversation(response_store_t *store, const char *name);

/**
 * Map conversation name to response ID.
 */
void response_store_set_conversation(response_store_t *store, const char *name, const char *response_id);

/* ── Idempotency Cache ────────────────────────────────────────────── */

/**
 * Initialize idempotency cache.
 * @param max_items Maximum items to cache
 * @param ttl_seconds Time-to-live in seconds
 * @return Cache handle, or NULL on failure
 */
idempotency_cache_t *idempotency_cache_new(int max_items, int ttl_seconds);

/**
 * Free idempotency cache.
 */
void idempotency_cache_free(idempotency_cache_t *cache);

/**
 * Get or compute a cached response.
 * @param cache Cache handle
 * @param key Idempotency key
 * @param fingerprint Request fingerprint
 * @param compute_fn Function to compute response if not cached
 * @param user_data User data passed to compute_fn
 * @return JSON string of response (caller must free), or NULL on error
 */
char *idempotency_cache_get_or_set(
    idempotency_cache_t *cache,
    const char *key,
    const char *fingerprint,
    char *(*compute_fn)(void *user_data),
    void *user_data
);

/* ── API Server Adapter Core ──────────────────────────────────────── */

/**
 * Create and initialize the API server adapter.
 * @param config Platform configuration
 * @return Adapter instance, or NULL on failure
 */
api_server_adapter_t *api_server_adapter_create(const gw_platform_config_t *config);

/**
 * Free the API server adapter.
 */
void api_server_adapter_free(api_server_adapter_t *adapter);

/**
 * Start the API server (connect).
 * @return true on success
 */
bool api_server_adapter_connect(api_server_adapter_t *adapter);

/**
 * Stop the API server (disconnect).
 */
void api_server_adapter_disconnect(api_server_adapter_t *adapter);

/**
 * Send a message (not used - HTTP request/response handles delivery).
 * Implements gw_platform_t.send
 */
gw_send_result_t api_server_adapter_send(
    api_server_adapter_t *adapter,
    const char *chat_id,
    const char *content,
    const char *reply_to,
    const json_t *metadata
);

/**
 * Get chat info.
 */
json_t *api_server_adapter_get_chat_info(api_server_adapter_t *adapter, const char *chat_id);

/* ── Internal handler functions (for HTTP routing) ────────────────── */

/* Health endpoints */
/* Port of Python gateway/platforms/api_server.py:_handle_health(). */
void api_server_handle_health(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin);
/* Port of Python gateway/platforms/api_server.py:_handle_health_detailed(). */
void api_server_handle_health_detailed(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin);

/* Models & Capabilities */
/* Port of Python gateway/platforms/api_server.py:_handle_models(). */
void api_server_handle_models(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin);

/* Port of Python gateway/platforms/api_server.py:_handle_capabilities(). */
void api_server_handle_capabilities(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin);

/* Skills & Toolsets */
/* Port of Python gateway/platforms/api_server.py:_handle_skills(). */
void api_server_handle_skills(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin);

/* Port of Python gateway/platforms/api_server.py:_handle_toolsets(). */
void api_server_handle_toolsets(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin);

/* Port of Python gateway/platforms/api_server.py:_handle_list_sessions(). */
/* Session endpoints */
/* Port of Python gateway/platforms/api_server.py:_handle_create_session(). */
void api_server_handle_list_sessions(api_server_adapter_t *adapter, int client_fd, const char *query);
/* Port of Python gateway/platforms/api_server.py:_handle_create_session(). */
void api_server_handle_create_session(api_server_adapter_t *adapter, int client_fd, const char *body);
/* Port of Python gateway/platforms/api_server.py:_handle_get_session(). */
void api_server_handle_get_session(api_server_adapter_t *adapter, int client_fd, const char *session_id);
/* Port of Python gateway/platforms/api_server.py:_handle_patch_session(). */
void api_server_handle_patch_session(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body);
/* Port of Python gateway/platforms/api_server.py:_handle_delete_session(). */
void api_server_handle_delete_session(api_server_adapter_t *adapter, int client_fd, const char *session_id);
/* Port of Python gateway/platforms/api_server.py:_handle_session_messages(). */
void api_server_handle_session_messages(api_server_adapter_t *adapter, int client_fd, const char *session_id);
/* Port of Python gateway/platforms/api_server.py:_handle_fork_session(). */
void api_server_handle_fork_session(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body);
void api_server_handle_session_chat(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body);
void api_server_handle_session_chat_stream(api_server_adapter_t *adapter, int client_fd, const char *session_id, const char *body);

/* Chat Completions */
void api_server_handle_chat_completions(api_server_adapter_t *adapter, int client_fd, const char *body, const char *query, const char *headers);
void api_server_handle_chat_completions_stream(api_server_adapter_t *adapter, int client_fd, const char *body, const char *headers);

/* Responses API */
void api_server_handle_responses(api_server_adapter_t *adapter, int client_fd, const char *body, const char *headers);
void api_server_handle_responses_stream(api_server_adapter_t *adapter, int client_fd, const char *body, const char *headers);
void api_server_handle_get_response(api_server_adapter_t *adapter, int client_fd, const char *response_id);
void api_server_handle_delete_response(api_server_adapter_t *adapter, int client_fd, const char *response_id);

/* Runs (structured event streaming) */
void api_server_handle_runs(api_server_adapter_t *adapter, int client_fd, const char *body, const char *headers);
void api_server_handle_get_run(api_server_adapter_t *adapter, int client_fd, const char *run_id);
void api_server_handle_run_events(api_server_adapter_t *adapter, int client_fd, const char *run_id);
void api_server_handle_run_approval(api_server_adapter_t *adapter, int client_fd, const char *run_id, const char *body);
void api_server_handle_stop_run(api_server_adapter_t *adapter, int client_fd, const char *run_id);

/* Cron Jobs API */
void api_server_handle_list_jobs(api_server_adapter_t *adapter, int client_fd, const char *query);
void api_server_handle_create_job(api_server_adapter_t *adapter, int client_fd, const char *body, const char *request_info);
void api_server_handle_get_job(api_server_adapter_t *adapter, int client_fd, const char *job_id);
void api_server_handle_update_job(api_server_adapter_t *adapter, int client_fd, const char *job_id, const char *body);
void api_server_handle_delete_job(api_server_adapter_t *adapter, int client_fd, const char *job_id);
void api_server_handle_pause_job(api_server_adapter_t *adapter, int client_fd, const char *job_id);
void api_server_handle_resume_job(api_server_adapter_t *adapter, int client_fd, const char *job_id);
void api_server_handle_run_job(api_server_adapter_t *adapter, int client_fd, const char *job_id);

/* ── Utility functions ────────────────────────────────────────────── */

/**
 * Check authentication (Bearer token).
 * @return NULL if auth OK, or error JSON string (caller must free)
 */
char *api_server_check_auth(api_server_adapter_t *adapter, const char *auth_header);

/**
 * Parse and validate X-Hermes-Session-Key header.
 * @return Session key string (caller must free), or NULL if not present/error
 * Set error_out to error JSON string on validation failure.
 */
char *api_server_parse_session_key_header(api_server_adapter_t *adapter, const char *header, char **error_out);

/**
 * Parse X-Hermes-Session-Id header.
 * @return Session ID string (caller must free), or NULL if not present
 * Set error_out to error JSON string on validation failure.
 */
char *api_server_parse_session_id_header(api_server_adapter_t *adapter, const char *header, char **error_out);

/**
 * Normalize chat content (flatten arrays to string).
 * Port of Python _normalize_chat_content.
 */
char *api_server_normalize_chat_content(const char *content, int max_depth, int depth);

/**
 * Normalize multimodal content (text + images).
 * Port of Python _normalize_multimodal_content.
 */
char *api_server_normalize_multimodal_content(const char *content);

/**
 * Check if content has visible payload.
 */
bool api_server_content_has_visible_payload(const char *content);

/**
 * Derive stable session ID from system prompt + first user message.
 */
char *api_server_derive_chat_session_id(const char *system_prompt, const char *first_user_message);

/**
 * Build request fingerprint for idempotency.
 */
char *api_server_make_request_fingerprint(const char *body, const char **keys, int key_count);

/**
 * Coerce port from config/env.
 */
int api_server_coerce_port(const char *value, int default_port);

/**
 * Coerce boolean from request values.
 */
bool api_server_coerce_request_bool(const char *value, bool default_val);

/* ── SSE Writer ───────────────────────────────────────────────────── */

sse_writer_t *sse_writer_new(int fd, const char *completion_id, const char *model, int created);
void sse_writer_free(sse_writer_t *writer);
void sse_write_event(sse_writer_t *writer, const char *event_type, const char *data);
void sse_write_chunk(sse_writer_t *writer, const char *content, int index);
void sse_write_finish(sse_writer_t *writer, const char *usage_json);

/* ── Run status management ────────────────────────────────────────── */

run_status_t *run_status_new(const char *run_id, const char *session_id, const char *model);
void run_status_free(run_status_t *status);
void run_status_update(run_status_t *status, const char *new_status, const char *event_json);
void run_status_send_event(run_status_t *status, const char *event_type, const char *data);
run_status_t *run_status_get(api_server_adapter_t *adapter, const char *run_id);
void run_status_set(api_server_adapter_t *adapter, const char *run_id, run_status_t *status);
void run_status_remove(api_server_adapter_t *adapter, const char *run_id);
void run_status_sweep(api_server_adapter_t *adapter);

/* ── SSE Queue ────────────────────────────────────────────────────── */

sse_queue_t *sse_queue_new(int capacity);
void sse_queue_free(sse_queue_t *q);
bool sse_queue_put(sse_queue_t *q, const char *item);
char *sse_queue_get(sse_queue_t *q, int timeout_ms);
void sse_queue_close(sse_queue_t *q);

/* ── Agent Run (for streaming responses) ──────────────────────────── */

typedef struct agent_run_args {
    const char *user_message;
    const char *conversation_history;
    const char *ephemeral_system_prompt;
    const char *session_id;
    const char *gateway_session_key;
    json_t **usage_out;
    char *result;
} agent_run_args_t;

void *agent_run_thread(void *arg);

char *api_server_run_agent(
    api_server_adapter_t *adapter,
    const char *user_message,
    const char *conversation_history,
    const char *ephemeral_system_prompt,
    const char *session_id,
    void (*stream_delta_cb)(const char *, void *),
    void (*tool_progress_cb)(const char *, const char *, const char *, const char *, void *),
    void (*tool_start_cb)(const char *, const char *, const char *, void *),
    void (*tool_complete_cb)(const char *, const char *, const char *, const char *, void *),
    const char *gateway_session_key,
    json_t **usage_out
);

/* ── HTTP Response / Session DB Helpers ───────────────────────────── */

struct api_server_adapter {
    gw_platform_config_t *config;
    char host[64];
    int port;
    char api_key[256];
    char cors_origins[10][256];
    int cors_count;
    char model_name[128];

    /* Server state */
    response_store_t *response_store;
    idempotency_cache_t *idempotency_cache;

    /* Run tracking */
    run_status_t *run_statuses[100];
    int run_status_count;
    pthread_mutex_t run_lock;

    /* Background sweep */
    pthread_t sweep_thread;
    volatile bool sweep_running;
};

struct response_store {
    int max_size;
    char *db_path;
    sqlite3 *conn;
    pthread_mutex_t lock;
};

struct idempotency_cache {
    int max_items;
    int ttl_seconds;
    pthread_mutex_t lock;
    struct cache_entry {
        char *key;
        char *fingerprint;
        char *response;
        time_t timestamp;
        struct cache_entry *next;
    } *head;
};

struct sse_queue {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    char **items;
    int head, tail, count, capacity;
    bool closed;
};

struct sse_writer {
    int fd;
    const char *completion_id;
    const char *model;
    int created;
};

struct run_status {
    char run_id[64];
    char status[32];
    double created_at;
    double updated_at;
    char *last_event_json;
    char *output;
    char *error;
    json_t *usage;
    char session_id[64];
    char model[128];
    sse_queue_t *event_queue;
    pthread_t run_thread;
    volatile bool running;
    char approval_session_key[128];
};

/* ── HTTP Response / Session DB Helpers ───────────────────────────── */

void send_json_response(int fd, int status, const char *json_body);
void send_error_response(int fd, int status, const char *message, const char *code);
void send_sse_headers(int fd);
void sse_write_event_fd(int fd, const char *event_type, const char *data);

db_t *get_session_db(void);

#endif /* HERMES_API_SERVER_ADAPTER_H */