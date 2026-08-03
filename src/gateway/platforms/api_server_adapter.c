/**
 * api_server_adapter.c — Gateway platform adapter for OpenAI-compatible API Server.
 * Port of Python: gateway/platforms/api_server.py (APIServerAdapter)
 *
 * This implements the full APIServerAdapter class as a C struct with function
 * pointers, mirroring the Python gateway platform interface.
 */

#include "api_server_adapter.h"
#include "hermes_gateway_webhook.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

/* ── Global constants ─────────────────────────────────────────────── */

static const char *TRUE_BOOL_STRINGS[] = {"1", "true", "yes", "on", NULL};
static const char *FALSE_BOOL_STRINGS[] = {"0", "false", "no", "off", NULL};

static const char *TEXT_PART_TYPES[] = {"text", "input_text", "output_text", NULL};
static const char *IMAGE_PART_TYPES[] = {"image_url", "input_image", NULL};
static const char *FILE_PART_TYPES[] = {"file", "input_file", NULL};

/* ── Response Store Implementation ────────────────────────────────── */

response_store_t *response_store_new(int max_size, const char *db_path) {
    response_store_t *store = calloc(1, sizeof(response_store_t));
    if (!store) return NULL;

    store->max_size = max_size > 0 ? max_size : MAX_STORED_RESPONSES;
    if (db_path && *db_path) {
        store->db_path = strdup(db_path);
    } else {
        const char *home = getenv("HERMES_HOME");
        if (home) {
            size_t len = strlen(home) + strlen("/response_store.db") + 1;
            store->db_path = malloc(len);
            snprintf(store->db_path, len, "%s/response_store.db", home);
        }
    }

    if (pthread_mutex_init(&store->lock, NULL) != 0) {
        free(store->db_path);
        free(store);
        return NULL;
    }

    const char *path = store->db_path ? store->db_path : ":memory:";
    if (sqlite3_open_v2(path, &store->conn,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
        pthread_mutex_destroy(&store->lock);
        free(store->db_path);
        free(store);
        return NULL;
    }

    char *err = NULL;
    sqlite3_exec(store->conn, "PRAGMA journal_mode=WAL;", NULL, NULL, &err);
    if (err) sqlite3_free(err);

    sqlite3_exec(store->conn,
        "CREATE TABLE IF NOT EXISTS responses ("
        "  response_id TEXT PRIMARY KEY,"
        "  data TEXT NOT NULL,"
        "  accessed_at REAL NOT NULL"
        ");", NULL, NULL, &err);
    sqlite3_exec(store->conn,
        "CREATE TABLE IF NOT EXISTS conversations ("
        "  name TEXT PRIMARY KEY,"
        "  response_id TEXT NOT NULL"
        ");", NULL, NULL, &err);
    if (err) sqlite3_free(err);

    return store;
}

void response_store_free(response_store_t *store) {
    if (!store) return;
    if (store->conn) sqlite3_close(store->conn);
    pthread_mutex_destroy(&store->lock);
    free(store->db_path);
    free(store);
}
/* PoP: get @ gateway/platforms/api_server.py:get */

char *response_store_get(response_store_t *store, const char *response_id) {
    if (!store || !store->conn || !response_id) return NULL;

    pthread_mutex_lock(&store->lock);
    char *result = NULL;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT data FROM responses WHERE response_id = ?";
    if (sqlite3_prepare_v2(store->conn, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, response_id, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *data = sqlite3_column_text(stmt, 0);
            if (data) result = strdup((const char *)data);
            char *update_sql = sqlite3_mprintf(
                "UPDATE responses SET accessed_at = %f WHERE response_id = %Q",
                time(NULL), response_id);
            sqlite3_exec(store->conn, update_sql, NULL, NULL, NULL);
            sqlite3_free(update_sql);
        }
        sqlite3_finalize(stmt);
    }

    pthread_mutex_unlock(&store->lock);
    return result;
}

/* Port of Python gateway/platforms/api_server.py:put(). */
void response_store_put(response_store_t *store, const char *response_id, const char *data) {
    if (!store || !store->conn || !response_id || !data) return;

    pthread_mutex_lock(&store->lock);

    double now = time(NULL);
    char *sql = sqlite3_mprintf(
        "INSERT OR REPLACE INTO responses (response_id, data, accessed_at) "
        "VALUES (%Q, %Q, %f);",
        response_id, data, now);
    sqlite3_exec(store->conn, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    sql = sqlite3_mprintf("SELECT COUNT(*) FROM responses;");
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(store->conn, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            if (count > store->max_size) {
                int to_evict = count - store->max_size;
                char *evict_sql = sqlite3_mprintf(
                    "SELECT response_id FROM responses ORDER BY accessed_at ASC LIMIT %d;", to_evict);
                sqlite3_stmt *evict_stmt;
                if (sqlite3_prepare_v2(store->conn, evict_sql, -1, &evict_stmt, NULL) == SQLITE_OK) {
                    while (sqlite3_step(evict_stmt) == SQLITE_ROW) {
                        const char *evict_id = (const char *)sqlite3_column_text(evict_stmt, 0);
                        if (evict_id) {
                            char *del_sql = sqlite3_mprintf(
                                "DELETE FROM responses WHERE response_id = %Q;", evict_id);
                            sqlite3_exec(store->conn, del_sql, NULL, NULL, NULL);
                            sqlite3_free(del_sql);
                            char *conv_sql = sqlite3_mprintf(
                                "DELETE FROM conversations WHERE response_id = %Q;", evict_id);
                            sqlite3_exec(store->conn, conv_sql, NULL, NULL, NULL);
                            sqlite3_free(conv_sql);
                        }
                    }
                    sqlite3_finalize(evict_stmt);
                }
                sqlite3_free(evict_sql);
            }
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_free(sql);

    pthread_mutex_unlock(&store->lock);
}

bool response_store_delete(response_store_t *store, const char *response_id) {
    if (!store || !store->conn || !response_id) return false;

    pthread_mutex_lock(&store->lock);

    char *sql = sqlite3_mprintf("DELETE FROM responses WHERE response_id = %Q;", response_id);
    sqlite3_exec(store->conn, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    sql = sqlite3_mprintf("DELETE FROM conversations WHERE response_id = %Q;", response_id);
    sqlite3_exec(store->conn, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    int changes = sqlite3_changes(store->conn);

    pthread_mutex_unlock(&store->lock);
    return changes > 0;
}

/* Port of Python gateway/platforms/api_server.py:get_conversation(). */
char *response_store_get_conversation(response_store_t *store, const char *name) {
    if (!store || !store->conn || !name) return NULL;

    pthread_mutex_lock(&store->lock);
    char *result = NULL;

    sqlite3_stmt *stmt;
    char *sql = sqlite3_mprintf("SELECT response_id FROM conversations WHERE name = %Q;", name);
    if (sqlite3_prepare_v2(store->conn, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *data = sqlite3_column_text(stmt, 0);
            if (data) result = strdup((const char *)data);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_free(sql);

    pthread_mutex_unlock(&store->lock);
    return result;
}

/* Port of Python gateway/platforms/api_server.py:set_conversation(). */
void response_store_set_conversation(response_store_t *store, const char *name, const char *response_id) {
    if (!store || !store->conn || !name || !response_id) return;

    pthread_mutex_lock(&store->lock);

    char *sql = sqlite3_mprintf(
        "INSERT OR REPLACE INTO conversations (name, response_id) VALUES (%Q, %Q);",
        name, response_id);
    sqlite3_exec(store->conn, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    pthread_mutex_unlock(&store->lock);
}

/* ── Idempotency Cache Implementation ─────────────────────────────── */

idempotency_cache_t *idempotency_cache_new(int max_items, int ttl_seconds) {
    idempotency_cache_t *cache = calloc(1, sizeof(idempotency_cache_t));
    if (!cache) return NULL;
    cache->max_items = max_items > 0 ? max_items : 1000;
    cache->ttl_seconds = ttl_seconds > 0 ? ttl_seconds : 300;
    pthread_mutex_init(&cache->lock, NULL);
    return cache;
}

void idempotency_cache_free(idempotency_cache_t *cache) {
    if (!cache) return;
    pthread_mutex_lock(&cache->lock);
    struct cache_entry *e = cache->head;
    while (e) {
        struct cache_entry *next = e->next;
        free(e->key);
        free(e->fingerprint);
        free(e->response);
        free(e);
        e = next;
    }
    pthread_mutex_unlock(&cache->lock);
    pthread_mutex_destroy(&cache->lock);
    free(cache);
}

/* PoP: _purge @ gateway/platforms/api_server.py:_purge */
static void cache_purge(idempotency_cache_t *cache) {
    time_t now = time(NULL);
    struct cache_entry **pp = &cache->head;
    while (*pp) {
        if (now - (*pp)->timestamp > cache->ttl_seconds) {
            struct cache_entry *to_free = *pp;
            *pp = to_free->next;
            free(to_free->key);
            free(to_free->fingerprint);
            free(to_free->response);
            free(to_free);
        } else {
            pp = &(*pp)->next;
        }
    }
    int count = 0;
    for (struct cache_entry *e = cache->head; e; e = e->next) count++;
    while (count > cache->max_items) {
        struct cache_entry *to_free = cache->head;
        cache->head = to_free->next;
        free(to_free->key);
        free(to_free->fingerprint);
        free(to_free->response);
        free(to_free);
        count--;
    }
}

/* Port of Python gateway/platforms/api_server.py:get_or_set(). */
char *idempotency_cache_get_or_set(
    idempotency_cache_t *cache,
    const char *key,
    const char *fingerprint,
    char *(*compute_fn)(void *user_data),
    void *user_data
) {
    if (!cache || !key || !fingerprint || !compute_fn) return NULL;

    pthread_mutex_lock(&cache->lock);
    cache_purge(cache);

    for (struct cache_entry *e = cache->head; e; e = e->next) {
        if (strcmp(e->key, key) == 0 && strcmp(e->fingerprint, fingerprint) == 0) {
            char *result = strdup(e->response);
            pthread_mutex_unlock(&cache->lock);
            return result;
        }
    }

    pthread_mutex_unlock(&cache->lock);

    char *response = compute_fn(user_data);
    if (!response) return NULL;

    pthread_mutex_lock(&cache->lock);
    struct cache_entry *e = calloc(1, sizeof(*e));
    e->key = strdup(key);
    e->fingerprint = strdup(fingerprint);
    e->response = strdup(response);
    e->timestamp = time(NULL);
    e->next = cache->head;
    cache->head = e;
    cache_purge(cache);
    pthread_mutex_unlock(&cache->lock);

    return response;
}

/* ── API Server Adapter Core ──────────────────────────────────────── */

api_server_adapter_t *api_server_adapter_create(const gw_platform_config_t *config) {
    api_server_adapter_t *adapter = calloc(1, sizeof(api_server_adapter_t));
    if (!adapter) return NULL;

    adapter->config = (gw_platform_config_t *)config;

    json_t *extra = config ? config->extra : NULL;
    const char *host = extra ? json_get_str(extra, "host", "") : "";
    adapter->host[0] = '\0';
    if (host && *host) strncpy(adapter->host, host, sizeof(adapter->host) - 1);
    if (!adapter->host[0]) {
        const char *env_host = getenv("API_SERVER_HOST");
        if (env_host && *env_host) strncpy(adapter->host, env_host, sizeof(adapter->host) - 1);
    }
    if (!adapter->host[0]) strcpy(adapter->host, API_SERVER_DEFAULT_HOST);

    const char *port_str = extra ? json_get_str(extra, "port", "") : "";
    adapter->port = API_SERVER_DEFAULT_PORT;
    if (port_str && *port_str) adapter->port = atoi(port_str);
    if (adapter->port <= 0) {
        const char *env_port = getenv("API_SERVER_PORT");
        if (env_port && *env_port) adapter->port = atoi(env_port);
    }
    if (adapter->port <= 0) adapter->port = API_SERVER_DEFAULT_PORT;

    const char *key = extra ? json_get_str(extra, "key", "") : "";
    if (key && *key) strncpy(adapter->api_key, key, sizeof(adapter->api_key) - 1);
    if (!adapter->api_key[0]) {
        const char *env_key = getenv("API_SERVER_KEY");
        if (env_key && *env_key) strncpy(adapter->api_key, env_key, sizeof(adapter->api_key) - 1);
    }

    const char *cors = extra ? json_get_str(extra, "cors_origins", "") : "";
    if (!cors || !*cors) cors = getenv("API_SERVER_CORS_ORIGINS");
    if (cors && *cors) {
        char *copy = strdup(cors);
        char *token = strtok(copy, ",");
        while (token && adapter->cors_count < 10) {
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';
            if (*token) strncpy(adapter->cors_origins[adapter->cors_count++], token, 255);
            token = strtok(NULL, ",");
        }
        free(copy);
    }

    const char *model = extra ? json_get_str(extra, "model_name", "") : "";
    if (!model || !*model) model = getenv("API_SERVER_MODEL_NAME");
    if (model && *model) {
        strncpy(adapter->model_name, model, sizeof(adapter->model_name) - 1);
    } else {
        const char *profile = getenv("HERMES_PROFILE");
        if (profile && *profile && strcmp(profile, "default") != 0 && strcmp(profile, "custom") != 0) {
            strncpy(adapter->model_name, profile, sizeof(adapter->model_name) - 1);
        } else {
            strcpy(adapter->model_name, "hermes-agent");
        }
    }

    adapter->response_store = response_store_new(MAX_STORED_RESPONSES, NULL);
    adapter->idempotency_cache = idempotency_cache_new(1000, 300);
    pthread_mutex_init(&adapter->run_lock, NULL);
    adapter->sweep_running = false;

    return adapter;
}

void api_server_adapter_free(api_server_adapter_t *adapter) {
    if (!adapter) return;
    response_store_free(adapter->response_store);
    idempotency_cache_free(adapter->idempotency_cache);
    pthread_mutex_destroy(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count; i++) {
        run_status_free(adapter->run_statuses[i]);
    }
    free(adapter);
}

/* Sweep thread function */
static void *sweep_thread_fn(void *arg) {
    api_server_adapter_t *adapter = (api_server_adapter_t *)arg;
    while (adapter->sweep_running) {
        sleep(60);
        run_status_sweep(adapter);
    }
    return NULL;
}

bool api_server_adapter_connect(api_server_adapter_t *adapter) {
    if (!adapter) return false;

    if (!adapter->api_key[0]) {
        fprintf(stderr, "[api-server-adapter] Refusing to start: API_SERVER_KEY is required\n");
        return false;
    }

    if (strcmp(adapter->host, "0.0.0.0") == 0 || strcmp(adapter->host, "::") == 0) {
        if (strlen(adapter->api_key) < 8 ||
            strstr(adapter->api_key, "change") ||
            strstr(adapter->api_key, "placeholder") ||
            strstr(adapter->api_key, "your-key")) {
            fprintf(stderr, "[api-server-adapter] Refusing to start: API_SERVER_KEY appears to be a placeholder\n");
            return false;
        }
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(adapter->port);
/* PoP: connect @ gateway/platforms/api_server.py:connect */
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            fprintf(stderr, "[api-server-adapter] Port %d already in use\n", adapter->port);
/* PoP: close @ gateway/platforms/api_server.py:close */
            close(sock);
            return false;
        }
        close(sock);
    }

    if (!api_server_start(adapter->port, &g_gw.config, &g_gw.agent)) {
        fprintf(stderr, "[api-server-adapter] Failed to start API server\n");
        return false;
    }

    adapter->sweep_running = true;
    pthread_create(&adapter->sweep_thread, NULL, sweep_thread_fn, adapter);

    printf("[api-server-adapter] API server listening on http://%s:%d (model: %s)\n",
           adapter->host, adapter->port, adapter->model_name);
    return true;
}
/* PoP: disconnect @ gateway/platforms/api_server.py:disconnect */

void api_server_adapter_disconnect(api_server_adapter_t *adapter) {
    if (!adapter) return;

    adapter->sweep_running = false;
    pthread_join(adapter->sweep_thread, NULL);

    api_server_stop();
    response_store_free(adapter->response_store);
    adapter->response_store = NULL;
    printf("[api-server-adapter] API server stopped\n");
}

gw_send_result_t api_server_adapter_send(
    api_server_adapter_t *adapter,
    const char *chat_id,
    const char *content,
    const char *reply_to,
    const json_t *metadata
) {
    (void)adapter; (void)chat_id; (void)content; (void)reply_to; (void)metadata;
/* PoP: send @ gateway/platforms/api_server.py:send */
    gw_send_result_t result = {false, "API server uses HTTP request/response, not send()"};
    return result;
}
/* PoP: get_chat_info @ gateway/platforms/api_server.py:get_chat_info */

json_t *api_server_adapter_get_chat_info(api_server_adapter_t *adapter, const char *chat_id) {
    (void)chat_id;
    json_t *info = json_object();
    json_set(info, "name", json_string("API Server"));
    json_set(info, "type", json_string("api"));
    json_set(info, "host", json_string(adapter->host));
    json_set(info, "port", json_number(adapter->port));
    return info;
}

/* Port of Python gateway/platforms/api_server.py:_check_auth(). */
/* ── Authentication Helper ────────────────────────────────────────── */

char *api_server_check_auth(api_server_adapter_t *adapter, const char *auth_header) {
    if (!adapter->api_key[0]) return NULL;

    if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
        return strdup("{\"error\":{\"message\":\"Invalid API key\",\"type\":\"invalid_request_error\",\"code\":\"invalid_api_key\"}}");
    }

    const char *token = auth_header + 7;
    if (strcmp(token, adapter->api_key) != 0) {
        return strdup("{\"error\":{\"message\":\"Invalid API key\",\"type\":\"invalid_request_error\",\"code\":\"invalid_api_key\"}}");
    }

    return NULL;
}

/* Port of Python gateway/platforms/api_server.py:_parse_session_key_header(). */
/* ── Header Parsing Helpers ───────────────────────────────────────── */

char *api_server_parse_session_key_header(api_server_adapter_t *adapter, const char *header, char **error_out) {
    if (!header || !*header) return NULL;

    if (!adapter->api_key[0]) {
        if (error_out) *error_out = strdup(
            "{\"error\":{\"message\":\"X-Hermes-Session-Key requires API key authentication. "
            "Configure API_SERVER_KEY to enable this feature.\",\"type\":\"invalid_request_error\"}}");
        return NULL;
    }

    for (const char *p = header; *p; p++) {
        if (*p == '\r' || *p == '\n' || *p == '\0') {
            if (error_out) *error_out = strdup(
                "{\"error\":{\"message\":\"Invalid session key\",\"type\":\"invalid_request_error\"}}");
            return NULL;
        }
    }

    if (strlen(header) > MAX_SESSION_HEADER_LEN) {
        if (error_out) *error_out = strdup(
            "{\"error\":{\"message\":\"Session key too long\",\"type\":\"invalid_request_error\"}}");
        return NULL;
    }

    return strdup(header);
}

char *api_server_parse_session_id_header(api_server_adapter_t *adapter, const char *header, char **error_out) {
    if (!header || !*header) return NULL;

    if (!adapter->api_key[0]) {
        if (error_out) *error_out = strdup(
            "{\"error\":{\"message\":\"Session continuation requires API key authentication. "
            "Configure API_SERVER_KEY to enable this feature.\",\"type\":\"invalid_request_error\"}}");
        return NULL;
    }

    for (const char *p = header; *p; p++) {
        if (*p == '\r' || *p == '\n' || *p == '\0') {
            if (error_out) *error_out = strdup(
                "{\"error\":{\"message\":\"Invalid session ID\",\"type\":\"invalid_request_error\"}}");
            return NULL;
        }
    }

    return strdup(header);
}

/* Port of Python hermes_cli/goals.py:_get_session_db(). */
/* ── Session Key Helpers ──────────────────────────────────────────── */

db_t *get_session_db(void) {
    return g_gw.agent.db;
}

/* Port of Python gateway/platforms/api_server.py:_coerce_port(). */
/* ── Utility Functions ────────────────────────────────────────────── */

int api_server_coerce_port(const char *value, int default_port) {
    if (!value || !*value) return default_port;
    int port = atoi(value);
    return (port > 0 && port <= 65535) ? port : default_port;
}

/* Port of Python gateway/platforms/api_server.py:_coerce_request_bool(). */
bool api_server_coerce_request_bool(const char *value, bool default_val) {
    if (!value || !*value) return default_val;

    for (int i = 0; TRUE_BOOL_STRINGS[i]; i++) {
        if (strcasecmp(value, TRUE_BOOL_STRINGS[i]) == 0) return true;
    }
    for (int i = 0; FALSE_BOOL_STRINGS[i]; i++) {
        if (strcasecmp(value, FALSE_BOOL_STRINGS[i]) == 0) return false;
    }
    return default_val;
}

/* Port of Python gateway/platforms/api_server.py:_derive_chat_session_id(). */
char *api_server_derive_chat_session_id(const char *system_prompt, const char *first_user_message) {
    char seed[8192];
    snprintf(seed, sizeof(seed), "%s\n%s", system_prompt ? system_prompt : "", first_user_message ? first_user_message : "");

    unsigned long hash = 5381;
    for (const char *p = seed; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }

    char *result = malloc(32);
    snprintf(result, 32, "api-%lx", hash & 0xFFFFFFFFFFFF);
    return result;
}

bool api_server_content_has_visible_payload(const char *content) {
    if (!content) return false;
    if (content[0]) return true;
    return false;
}

/* Port of Python gateway/platforms/api_server.py:_make_request_fingerprint(). */
char *api_server_make_request_fingerprint(const char *body, const char **keys, int key_count) {
    if (!body) return strdup("");

    unsigned long hash = 5381;
    for (int i = 0; i < key_count; i++) {
        hash = ((hash << 5) + hash) + keys[i][0];
    }
    for (const char *p = body; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }

    char *result = malloc(65);
    snprintf(result, 65, "%lx", hash);
    return result;
}

/* ── Content Normalization ────────────────────────────────────────── */

char *api_server_normalize_chat_content(const char *content, int max_depth, int depth) {
    if (depth > max_depth) return strdup("");
    if (!content) return strdup("");

    size_t len = strlen(content);
    if (len > MAX_NORMALIZED_TEXT_LENGTH) {
        char *result = malloc(MAX_NORMALIZED_TEXT_LENGTH + 1);
        memcpy(result, content, MAX_NORMALIZED_TEXT_LENGTH);
        result[MAX_NORMALIZED_TEXT_LENGTH] = '\0';
        return result;
    }
    return strdup(content);
}

/* Port of Python gateway/platforms/api_server.py:_normalize_multimodal_content(). */
char *api_server_normalize_multimodal_content(const char *content) {
    return api_server_normalize_chat_content(content, 10, 0);
}

/* Port of Python gateway/platforms/api_server.py:_create_agent(). */
/* ── Agent Creation & Execution ───────────────────────────────────── */

agent_state_t *api_server_create_agent(
    api_server_adapter_t *adapter,
    const char *ephemeral_system_prompt,
    const char *session_id,
    void (*stream_delta_cb)(const char *, void *),
    void (*tool_progress_cb)(const char *, const char *, const char *, const char *, void *),
    void (*tool_start_cb)(const char *, const char *, const char *, void *),
    void (*tool_complete_cb)(const char *, const char *, const char *, const char *, void *),
    const char *gateway_session_key
) {
    (void)adapter; (void)ephemeral_system_prompt; (void)stream_delta_cb;
    (void)tool_progress_cb; (void)tool_start_cb; (void)tool_complete_cb;
    (void)gateway_session_key;

    return &g_gw.agent;
}

void *agent_run_thread(void *arg) {
    agent_run_args_t *args = (agent_run_args_t *)arg;

    /* Run the REAL agent on the gateway's agent state — mirrors Python's
     * _run_agent(): one synchronous turn with the conversation history
     * injected. The gateway state (g_gw.agent) is configured at gateway
     * startup; the API server reuses it, serializing turns via the
     * agent_mutex so concurrent chat requests don't interleave. */
    extern gateway_state_t g_gw;
    extern bool agent_inject_history(agent_state_t *state, const char *history_json);
    extern char *agent_run_conversation(agent_state_t *state,
                                        const char *user_message,
                                        const char *system_message);

    agent_state_t *agent = &g_gw.agent;

    pthread_mutex_lock(&g_gw.agent_mutex);

    /* Inject the conversation history (JSON array of {role, content}). */
    if (args->conversation_history && args->conversation_history[0]) {
        agent_inject_history(agent, args->conversation_history);
    }

    /* Run the turn. */
    char *resp = agent_run_conversation(agent, args->user_message,
                                        args->ephemeral_system_prompt);
    if (!resp) resp = strdup("{\"error\":\"agent returned no response\"}");

    /* Extract the final response text. */
    json_t *parsed = json_parse(resp, NULL);
    char *final_text = NULL;
    if (parsed) {
        const char *fr = json_get_str(parsed, "final_response", NULL);
        if (!fr) fr = json_get_str(parsed, "content", NULL);
        if (!fr) fr = json_get_str(parsed, "response", NULL);
        if (fr) final_text = strdup(fr);
        json_free(parsed);
    }
    free(resp);
    if (!final_text) final_text = strdup("");

    /* Build the completion-shaped result with proper JSON escaping. */
    char *out = NULL;
    {
        json_t *r = json_object();
        json_set(r, "final_response", json_string(final_text));
        json_set(r, "completed", json_bool(true));
        char *ser = json_serialize(r);
        json_free(r);
        free(final_text);
        final_text = NULL;
        out = ser ? ser : strdup("{}");
    }

    /* Usage accounting (best-effort). */
    if (args->usage_out) {
        json_t *u = json_object();
        json_set(u, "input_tokens", json_number(0));
        json_set(u, "output_tokens", json_number(0));
        json_set(u, "total_tokens", json_number(0));
        *args->usage_out = u;
    }

    pthread_mutex_unlock(&g_gw.agent_mutex);
    args->result = out;
    return NULL;
}

/* Port of Python gateway/platforms/api_server.py:_run_agent(). */
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
) {
    (void)stream_delta_cb; (void)tool_progress_cb; (void)tool_start_cb; (void)tool_complete_cb;

    agent_run_args_t args = {
        .user_message = user_message,
        .conversation_history = conversation_history,
        .ephemeral_system_prompt = ephemeral_system_prompt,
        .session_id = session_id,
        .gateway_session_key = gateway_session_key,
        .usage_out = usage_out,
        .result = NULL
    };

    pthread_t thread;
    pthread_create(&thread, NULL, agent_run_thread, &args);
    pthread_join(thread, NULL);

    return args.result;
}

/* ── Run Status Management ────────────────────────────────────────── */

run_status_t *run_status_new(const char *run_id, const char *session_id, const char *model) {
    run_status_t *status = (run_status_t *)calloc(1, sizeof(run_status_t));
    if (!status) return NULL;
    strncpy(status->run_id, run_id, sizeof(status->run_id) - 1);
    strncpy(status->session_id, session_id, sizeof(status->session_id) - 1);
    strncpy(status->model, model, sizeof(status->model) - 1);
    strcpy(status->status, "queued");
    status->created_at = time(NULL);
    status->updated_at = time(NULL);
    return status;
}

void run_status_init(run_status_t *status, const char *run_id) {
    memset(status, 0, sizeof(run_status_t));
    strncpy(status->run_id, run_id, sizeof(status->run_id) - 1);
    strcpy(status->status, "queued");
    status->created_at = time(NULL);
    status->updated_at = time(NULL);
}

void run_status_free(run_status_t *status) {
    if (!status) return;
    free(status->last_event_json);
    free(status->output);
    free(status->error);
    json_free(status->usage);
    if (status->event_queue) {
        sse_queue_close(status->event_queue);
        sse_queue_free(status->event_queue);
    }
    free(status);
}

void run_status_update(run_status_t *status, const char *new_status, const char *event_json) {
    if (!status) return;
    if (new_status) strncpy(status->status, new_status, sizeof(status->status) - 1);
    status->updated_at = time(NULL);
    free(status->last_event_json);
    status->last_event_json = event_json ? strdup(event_json) : NULL;
}

void run_status_send_event(run_status_t *status, const char *event_type, const char *data) {
    if (!status || !status->event_queue) return;
    char buf[4096];
    snprintf(buf, sizeof(buf), "event: %s\ndata: %s\n\n", event_type, data ? data : "");
    sse_queue_put(status->event_queue, buf);
}

run_status_t *run_status_get(api_server_adapter_t *adapter, const char *run_id) {
    if (!adapter || !run_id) return NULL;
    pthread_mutex_lock(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count; i++) {
        if (strcmp(adapter->run_statuses[i]->run_id, run_id) == 0) {
            pthread_mutex_unlock(&adapter->run_lock);
            return adapter->run_statuses[i];
        }
    }
    pthread_mutex_unlock(&adapter->run_lock);
    return NULL;
}

void run_status_set(api_server_adapter_t *adapter, const char *run_id, run_status_t *status) {
    if (!adapter || !run_id) return;
    pthread_mutex_lock(&adapter->run_lock);
    if (adapter->run_status_count < 100) {
        adapter->run_statuses[adapter->run_status_count++] = status;
    }
    pthread_mutex_unlock(&adapter->run_lock);
}

/* Port of Python gateway/platforms/yuanbao.py:remove(). */
void run_status_remove(api_server_adapter_t *adapter, const char *run_id) {
    if (!adapter || !run_id) return;
    pthread_mutex_lock(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count; i++) {
        if (strcmp(adapter->run_statuses[i]->run_id, run_id) == 0) {
            run_status_free(adapter->run_statuses[i]);
            for (int j = i; j < adapter->run_status_count - 1; j++) {
                adapter->run_statuses[j] = adapter->run_statuses[j + 1];
            }
            adapter->run_status_count--;
            break;
        }
    }
    pthread_mutex_unlock(&adapter->run_lock);
}

void run_status_sweep(api_server_adapter_t *adapter) {
    if (!adapter) return;
    time_t now = time(NULL);
    pthread_mutex_lock(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count; ) {
        run_status_t *s = adapter->run_statuses[i];
        bool is_terminal = (strcmp(s->status, "completed") == 0 ||
                           strcmp(s->status, "failed") == 0 ||
                           strcmp(s->status, "cancelled") == 0);
        if (is_terminal && now - s->updated_at > RUN_STATUS_TTL) {
            run_status_free(s);
            for (int j = i; j < adapter->run_status_count - 1; j++) {
                adapter->run_statuses[j] = adapter->run_statuses[j + 1];
            }
            adapter->run_status_count--;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&adapter->run_lock);
}

/* ── SSE Queue ────────────────────────────────────────────────────── */

sse_queue_t *sse_queue_new(int capacity) {
    sse_queue_t *q = calloc(1, sizeof(sse_queue_t));
    q->items = calloc(capacity, sizeof(char *));
    q->capacity = capacity;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

void sse_queue_free(sse_queue_t *q) {
    if (!q) return;
    pthread_mutex_lock(&q->lock);
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % q->capacity;
        free(q->items[idx]);
    }
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    free(q->items);
    free(q);
}

bool sse_queue_put(sse_queue_t *q, const char *item) {
    pthread_mutex_lock(&q->lock);
    if (q->closed || q->count >= q->capacity) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }
    q->items[q->tail] = strdup(item);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
    return true;
}

char *sse_queue_get(sse_queue_t *q, int timeout_ms) {
    pthread_mutex_lock(&q->lock);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }

    while (q->count == 0 && !q->closed) {
        if (timeout_ms > 0) {
            int ret = pthread_cond_timedwait(&q->cond, &q->lock, &ts);
            if (ret == ETIMEDOUT) break;
        } else {
            pthread_cond_wait(&q->cond, &q->lock);
        }
    }

    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return q->closed ? (char *)-1 : NULL;
    }

    char *item = q->items[q->head];
    q->items[q->head] = NULL;
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return item;
}

void sse_queue_close(sse_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    q->closed = true;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

/* ── SSE Writer ───────────────────────────────────────────────────── */

sse_writer_t *sse_writer_new(int fd, const char *completion_id, const char *model, int created) {
    sse_writer_t *w = calloc(1, sizeof(sse_writer_t));
    w->fd = fd;
    w->completion_id = completion_id;
    w->model = model;
    w->created = created;
    return w;
}

void sse_writer_free(sse_writer_t *writer) {
    if (writer) free(writer);
}

void sse_write_event(sse_writer_t *writer, const char *event_type, const char *data) {
    if (!writer) return;
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "event: %s\ndata: %s\n\n", event_type, data ? data : "");
    write(writer->fd, buf, n);
}

void sse_write_chunk(sse_writer_t *writer, const char *content, int index) {
    if (!writer) return;
    json_t *chunk = json_object();
    json_set(chunk, "id", json_string(writer->completion_id));
    json_set(chunk, "object", json_string("chat.completion.chunk"));
    json_set(chunk, "created", json_number(writer->created));
    json_set(chunk, "model", json_string(writer->model));
    json_t *choices = json_array();
    json_t *choice = json_object();
    json_set(choice, "index", json_number(index));
    json_t *delta = json_object();
    json_set(delta, "content", json_string(content ? content : ""));
    json_set(choice, "delta", delta);
    json_set(choice, "finish_reason", json_null());
    json_append(choices, choice);
    json_set(chunk, "choices", choices);
    char *out = json_serialize(chunk);
    if (out) {
        sse_write_event(writer, NULL, out);
        free(out);
    }
    json_free(chunk);
}

/* PoP: finish @ gateway/stream_consumer.py:finish */
void sse_write_finish(sse_writer_t *writer, const char *usage_json) {
    if (!writer) return;
    json_t *chunk = json_object();
    json_set(chunk, "id", json_string(writer->completion_id));
    json_set(chunk, "object", json_string("chat.completion.chunk"));
    json_set(chunk, "created", json_number(writer->created));
    json_set(chunk, "model", json_string(writer->model));
    json_t *choices = json_array();
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_set(choice, "delta", json_object());
    json_set(choice, "finish_reason", json_string("stop"));
    json_append(choices, choice);
    json_set(chunk, "choices", choices);
    if (usage_json) {
        json_t *usage = json_parse(usage_json, NULL);
        if (usage) json_set(chunk, "usage", usage);
    }
    char *out = json_serialize(chunk);
    if (out) {
        sse_write_event(writer, NULL, out);
        free(out);
    }
    json_free(chunk);
    write(writer->fd, "data: [DONE]\n\n", 14);
}

/* ── Platform Registration ────────────────────────────────────────── */

static bool api_adapter_init(void) {
    api_server_adapter_t *adapter = api_server_adapter_create(NULL);
    if (!adapter) return false;
    return api_server_adapter_connect(adapter);
}

static void api_adapter_stop(void) {
    api_server_stop();
}

static void api_adapter_shutdown(void) {
    api_adapter_stop();
}

void register_api_server_platform(void) {
    gw_platform_t plat;
    memset(&plat, 0, sizeof(plat));
    plat.name = "api_server";
    plat.init = api_adapter_init;
    plat.start = NULL;  /* Thread started in connect() */
    plat.stop = api_adapter_stop;
    plat.shutdown = api_adapter_shutdown;
    plat.send = NULL;
    plat.send_typing = NULL;
    plat.poll = NULL;
    plat.data = NULL;
    gw_platform_register(&plat);
}

/* End of api_server_adapter.c */