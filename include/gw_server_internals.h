/*
 * gw_server_internals.h -- cross-module gateway helpers promoted
 * from gateway/server.c. Every moved function and every kept
 * server.c function referenced by an extracted module is declared
 * here (non-static), plus shared file-scope macros/globals.
 * server.c and each extracted module include this header.
 */

#ifndef GW_SERVER_INTERNALS_H
#define GW_SERVER_INTERNALS_H

#include "hermes_gateway.h"

/* shared macros (were file-scope in server.c) */
#define GW_LOG_MAX_BYTES (10 * 1024 * 1024)  /* 10 MB before rotation */
#define GW_LOG_PATH_MAX 512
#define GW_HOOKS_MAX 16
#define GW_EVENT_LISTENERS_MAX 16

/* ==================================================================
 * Shared cross-module state (was file-scope static in server.c).
 * These are referenced by the extracted gateway modules, so they
 * are promoted to named, non-static, extern-visible types here.
 * server.c owns the definitions; each module includes this header.
 * ================================================================== */

/* Clarify/approval async response collectors (anonymous structs -> named). */
typedef struct {
    bool            pending;
    char            platform[32];
    char            chat_id[128];
    char            session_key[256];
    char            clarify_id[64];
    char            response[4096];
    pthread_mutex_t mutex;
    pthread_cond_t  cond;   /* signaled when response received */
    char *(*poll_fn)(const char *chat_id);
    int           poll_interval;
    char            choices[4][256];
    int             n_choices;
    bool            has_choices;
} gw_clarify_state_t;

typedef struct {
    bool            pending;
    char            platform[32];
    char            chat_id[128];
    char            response[64];
    pthread_mutex_t mutex;
    pthread_cond_t  cond;   /* signaled when response received */
    char *(*poll_fn)(const char *chat_id);
    int           poll_interval;
} gw_approval_state_t;

/* Hook registry + event bus (anonymous structs -> named). */
typedef struct {
    gw_hook_t pre_send[GW_HOOKS_MAX];
    void     *pre_send_data[GW_HOOKS_MAX];
    int       pre_send_count;
    gw_hook_t post_receive[GW_HOOKS_MAX];
    void     *post_receive_data[GW_HOOKS_MAX];
    int       post_receive_count;
    gw_hook_t interceptor[GW_HOOKS_MAX];
    void     *interceptor_data[GW_HOOKS_MAX];
    int       interceptor_count;
} gw_hooks_t;

typedef struct {
    gw_event_listener_t listeners[GW_EVENT_LISTENERS_MAX];
    void               *data[GW_EVENT_LISTENERS_MAX];
    int                 count;
} gw_event_bus_t;

/* Tool-event / stream callback context (typedef in server.c). */
typedef struct {
    const char *platform;
    const char *chat_id;
    double      last_status_ts; /* monotonic time of last status send */
    double      last_stream_ts; /* monotonic time of last stream update */
    char        stream_buf[512]; /* accumulated stream tokens (truncated) */
    int         stream_len;      /* total chars accumulated so far */
} gw_status_ctx_t;

/* Definitions live in server.c. */
extern gw_clarify_state_t  g_gw_clarify;
extern gw_approval_state_t g_gw_approval;
extern gw_hooks_t          gw_hooks;
extern gw_event_bus_t      gw_event_bus;
extern FILE               *g_gw_log_fp;
extern char                g_gw_log_path[GW_LOG_PATH_MAX];

/* promoted statics (defined in their home module / server.c) */
    double gw_mono_time(void);
    void gw_queue_init(void);
    bool gw_queue_push(const char *platform, const char *chat_id, const char *text, const char *thread_id);
    bool gw_queue_pop(gateway_msg_t *msg);
    int gw_queue_depth(void);
    void gw_queue_drain_all(void);
    void gw_clarify_set_poll(char *(*fn)(const char *chat_id), int interval_sec);
    void gw_clarify_begin(const char *platform, const char *chat_id, const char *session_key, const char *clarify_id, const char (*choices)[256], int n_choices);
    bool gw_clarify_match(const char *platform, const char *chat_id, const char *text);
    bool gw_clarify_check_response(const char *platform, const char *chat_id, const char *text);
    void gw_approval_set_poll(char *(*fn)(const char *chat_id), int interval_sec);
    void gw_approval_set_context(const char *platform, const char *chat_id);
    void gw_approval_begin(const char *platform, const char *chat_id);
    bool gw_approval_match(const char *platform, const char *chat_id, const char *text);
    bool gw_approval_check_response(const char *platform, const char *chat_id, const char *text);
    void gw_log_open(void);
    void gw_log_close(void);
    void gw_rate_limit_init(int idx, double tokens_per_sec, double max_burst);
    bool gw_rate_limit_check(int idx);
    void gw_pool_return_client(http_client_t *client, const char *endpoint);
    void gw_pool_cleanup(void);
    void gw_set_keepalive(int plat_idx, double keepalive_sec);
    bool gw_dedup_check(const char *message_id);
    void gw_dedup_add(const char *message_id);
    void gw_batch_accumulate(const char *platform, const char *chat_id, const char *fragment);
    void gw_batch_flush(void);
    double gw_cooldown_remaining(int plat_idx);
    void gw_cooldown_mark(int plat_idx);
    double gw_reconnect_delay(int plat_idx);
    void gw_reconnect_reset(int plat_idx);
    bool gw_set_proxy(int plat_idx, const char *proxy_url);
    void gw_set_group_observe(const char *prefix, bool enabled);
    void gw_observe_append(const char *platform, const char *chat_id, const char *text);
    void gw_register_pre_send(gw_hook_t hook, void *userdata);
    void gw_register_post_receive(gw_hook_t hook, void *userdata);
    void gw_register_interceptor(gw_hook_t hook, void *userdata);
    void gw_event_register(gw_event_listener_t listener, void *userdata);
    void gw_event_emit(const char *event_type, json_node_t *data);
    bool gw_retry_with_backoff(bool (*api_call)(void *ctx), void *ctx, int max_retries, int base_delay_ms);
    bool gw_refresh_token(int plat_idx);
    void gateway_send_fallback(const char *platform, const char *target, const char *text);
    int session_source_description(const gw_session_source_t *src, char *buf, size_t sz);
    void session_source_set(gw_session_source_t *src, const char *platform, const char *chat_id, const char *chat_name, const char *chat_type, const char *user_id, const char *user_name, const char *thread_id, const char *chat_topic, const char *user_id_alt, const char *chat_id_alt, const char *guild_id, const char *parent_chat_id, const char *message_id, bool is_bot);
    bool gw_session_set_source(const char *platform, const char *chat_id, const gw_session_source_t *source);
    void source_cache_put(const char *key, const gw_session_source_t *source);
    void pii_hash(const char *input, char *out, size_t out_sz);
    bool discord_tools_loaded(void);
    bool session_should_reset(double session_sec);
    void session_free(int idx);
    gw_session_entry_t *session_at(int idx);
    bool is_shared_multi_user_session(const gw_session_source_t *src, bool group_sessions_per_user, bool thread_sessions_per_user);
    void build_session_key(char *buf, size_t sz, const char *platform, const char *chat_id);
    int session_find(const char *platform, const char *chat_id);
    int session_create(const char *platform, const char *chat_id);
    int session_get_or_create(const char *platform, const char *chat_id);
    void session_save_all(void);
    void session_cleanup_idle(void);
    int session_find_by_key(const char *session_key);
    int lookup_by_session_id(const char *session_id);
    const char *peek_session_id(int session_idx);
    void set_model_override(const char *session_key, const char *model);
    const char *get_model_override(const char *session_key);
    void rewind_session(const char *session_key, int turn_count);
    bool gw_try_send_media(const char *platform, const char *target, const char *text);
    void gateway_send(const char *platform, const char *target, const char *text);
    void gateway_send_typing(const char *platform, const char *target);
    void gw_platform_register(const gw_platform_t *plat);
    int gw_platform_get_count(void);
    bool gw_platform_send(const char *platform_name, const char *chat_id, const char *text);
    void gw_platform_send_typing(const char *platform_name, const char *chat_id);
    bool gw_platform_send_reaction(const char *platform_name, const char *chat_id, const char *message_id, const char *emoji);
    bool telegram_vtable_send_reaction(const char *chat_id, const char *message_id, const char *emoji);
    void poll_platform_shutdown(void);
    void gw_platform_shutdown_all(void);
    int gateway_tool_event_cb(const char *event_type, const char *tool_name, const char *tool_args, void *user_data);
    int gateway_stream_cb(const char *token, void *user_data);
    void process_update(const char *platform, const char *chat_id, const char *text);
    void handle_signal(int sig);
    bool setup_telegram(void);
    bool setup_discord(void);
    bool setup_slack(void);
    bool setup_matrix(void);
    bool setup_mattermost(void);
    bool setup_webhook(void);
    bool setup_whatsapp(void);
    bool setup_email(void);
    bool setup_signal(void);
    bool setup_api_server(void);
    bool setup_ha(void);
    bool setup_sms(void);
    bool setup_feishu(void);
    bool setup_wecom(void);
    bool setup_dingtalk(void);
    bool setup_qqbot(void);
    bool setup_bluebubbles(void);
    int get_webhook_port(void);
    bool setup_msgraph_webhook(void);
    bool setup_weixin(void);
    bool setup_yuanbao(void);
    int cmd_gateway_status(void);
    int cmd_gateway_list(void);

    /* hook pipeline helpers (defined in server.c, used by gw_dispatch.c) */
    char *gw_apply_pre_send_hooks(const char *platform, const char *text);
    char *gw_apply_post_receive_hooks(const char *platform, const char *chat_id, const char *text);
    char *gw_apply_interceptors(const char *platform, const char *chat_id, const char *text);
    char *gw_strip_all_formatting(const char *text);
    gw_platform_t *gw_platform_find(const char *name);

#endif /* GW_SERVER_INTERNALS_H */
