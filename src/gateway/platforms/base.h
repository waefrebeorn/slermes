/*
 * gateway/platforms/base.h — Base platform functionality header.
 *
 * Port of Python gateway/platforms/base.py.
 *
 * Provides common platform utilities: UTF-16 length, proxy handling,
 * media caching, message formatting, media extraction, platform vtable helpers,
 * and the BasePlatformAdapter vtable.
 */

#ifndef HERMES_GATEWAY_PLATFORMS_BASE_H
#define HERMES_GATEWAY_PLATFORMS_BASE_H

#include "hermes_gateway.h"
#include "hermes_gateway_config.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  UTF-16 helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: utf16_len */
size_t utf16_len(const char *s);

/* Port of Python: _prefix_within_utf16_limit */
char *gw_prefix_within_utf16_limit(const char *s, size_t limit);

/* Port of Python: _custom_unit_to_cp */
int custom_unit_to_cp(const char *s, int len, int budget,
                       int (*len_fn)(const char *, int));

/* ================================================================
 *  Float/env helpers
 * ================================================================ */

/* Port of Python: _float_env */
double float_env(const char *name, double default_value);

/* ================================================================
 *  Media cache helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: _looks_like_image */
bool looks_like_image(const char *url);

/* Port of Python: _looks_like_image (bytes variant) */
bool looks_like_image_bytes(const unsigned char *data, size_t len);

/* Port of Python: cache_image_from_bytes */
char *cache_image_from_bytes(const unsigned char *data, size_t len, const char *ext);

/* Port of Python: cache_audio_from_bytes */
char *cache_audio_from_bytes(const unsigned char *data, size_t len, const char *ext);

/* Port of Python: cache_video_from_bytes */
char *cache_video_from_bytes(const unsigned char *data, size_t len, const char *ext);

/* Port of Python: cache_document_from_bytes */
char *cache_document_from_bytes(const unsigned char *data, size_t len, const char *filename);

/* Port of Python: cleanup_image_cache */
int cleanup_image_cache(int max_age_hours);

/* Port of Python: cleanup_audio_cache */
int cleanup_audio_cache(int max_age_hours);

/* Port of Python: cleanup_video_cache */
int cleanup_video_cache(int max_age_hours);

/* Port of Python: cleanup_document_cache */
int cleanup_document_cache(int max_age_hours);

/* Port of Python: get_screenshot_cache_dir */
char *get_screenshot_cache_dir(void);

/* Port of Python: cleanup_screenshot_cache */
int cleanup_screenshot_cache(int max_age_hours);

/* ================================================================
 *  Session/source helpers
 * ================================================================ */

/* Port of Python: build_source */
json_node_t *gw_build_source(const char *platform, const char *chat_id,
                              const char *chat_name, const char *chat_type,
                              const char *user_id, const char *user_name,
                              const char *thread_id);

/* ================================================================
 *  Proxy helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: _detect_macos_system_proxy */
char *detect_macos_system_proxy(void);

/* Port of Python: _split_host_port */
typedef struct {
    char *host;
    int port;
    bool has_port;
} gw_host_port_t;

gw_host_port_t gw_split_host_port(const char *value);

/* Port of Python: should_bypass_proxy */
bool should_bypass_proxy(const char *target_hosts);

/* Port of Python: proxy_kwargs_for_aiohttp (simplified for C) */
typedef struct {
    char *connector;  // For SOCKS: serialized ProxyConnector info
    char *proxy;      // For HTTP: proxy URL
    bool has_connector;
    bool has_proxy;
} gw_proxy_kwargs_t;

/* Port of Python: proxy_kwargs_for_aiohttp (simplified for C) */
gw_proxy_kwargs_t proxy_kwargs_for_aiohttp(const char *proxy_url);
/* ================================================================
 *  Media delivery path validation
 * ================================================================ */

/* Port of Python: validate_media_delivery_path */
char *validate_media_delivery_path(const char *path);

/* Port of Python: _media_delivery_strict_mode */
bool gw_media_delivery_strict_mode(void);

/* Port of Python: _media_delivery_recency_seconds */
double gw_media_delivery_recency_seconds(void);

/* ================================================================
 *  Message formatting and media extraction
 * ================================================================ */

/* Port of Python: format_message */
char *gw_format_message(const char *text, bool markdown);

/* Port of Python: extract_images */
typedef struct {
    char **urls;
    char **alt_texts;
    size_t count;
} gw_image_list_t;

gw_image_list_t gw_extract_images(const char *content);
void gw_image_list_free(gw_image_list_t *list);

/* Port of Python: extract_media */
typedef struct {
    char **paths;
    bool *is_voice;
    size_t count;
} gw_media_list_t;

gw_media_list_t gw_extract_media(const char *content);
void gw_media_list_free(gw_media_list_t *list);

/* Port of Python: extract_local_files */
typedef struct {
    char **paths;
    size_t count;
} gw_path_list_t;

gw_path_list_t gw_extract_local_files(const char *content);
void gw_path_list_free(gw_path_list_t *list);

/* Port of Python: _strip_media_directives */
char *gw_strip_media_directives(const char *text);

/* Port of Python: _merge_caption */
char *gw_merge_caption(const char *existing_text, const char *new_text);

/* Port of Python: truncate_message */
typedef struct {
    char **chunks;
    size_t count;
} gw_chunk_list_t;

gw_chunk_list_t gw_platform_truncate_message(const char *content, int max_length, int (*len_fn)(const char *));
void gw_chunk_list_free(gw_chunk_list_t *list);

/* ================================================================
 *  Session/typing helpers
 * ================================================================ */

/* Port of Python: _get_human_delay */
double gw_get_human_delay(void);

/* Port of Python: _reply_anchor_for_event */
char *gw_reply_anchor_for_event(const json_node_t *event_source, const char *reply_to_message_id);

/* Port of Python: _thread_metadata_for_source */
json_node_t *gw_thread_metadata_for_source(const json_node_t *source, const char *reply_to_message_id);

/* ================================================================
 *  CachedMedia struct (Port of Python gateway.platforms.base.CachedMedia)
 * ================================================================ */

typedef struct {
    char *path;
    char *media_type;
    char *kind;           /* "image" | "video" | "audio" | "document" */
    char *display_name;
} gw_cached_media_t;

/* Port of Python: CachedMedia.__init__ */
gw_cached_media_t *gw_cached_media_new(const char *path, const char *media_type,
                                        const char *kind, const char *display_name);
void gw_cached_media_free(gw_cached_media_t *media);

/* Port of Python: cache_media_bytes */
gw_cached_media_t *gw_cache_media_bytes(const unsigned char *data, size_t len,
                                         const char *filename, const char *mime_type,
                                         const char *default_kind);

/* Port of Python: _resolve_media_ext */
char *gw_resolve_media_ext(const char *filename, const char *mime_type);

/* ================================================================
 *  BasePlatformAdapter vtable
 * ================================================================ */

/* Forward declaration */
typedef struct gw_base_platform_adapter gw_base_platform_adapter_t;

/* Port of Python: SendResult */
typedef struct {
    bool success;
    char *message_id;
    char *error;
    json_node_t *raw_response;
    bool retryable;
    char **continuation_message_ids;
    size_t continuation_count;
} gw_send_result_t;

/* Port of Python: SendResult.__init__ */
gw_send_result_t gw_send_result_new(bool success, const char *message_id, const char *error,
                                     json_node_t *raw_response, bool retryable);
void gw_send_result_free(gw_send_result_t *result);

/* Port of Python: MessageType */
typedef enum {
    GW_MSG_TEXT = 0,
    GW_MSG_LOCATION = 1,
    GW_MSG_PHOTO = 2,
    GW_MSG_VIDEO = 3,
    GW_MSG_AUDIO = 4,
    GW_MSG_VOICE = 5,
    GW_MSG_DOCUMENT = 6,
    GW_MSG_STICKER = 7,
    GW_MSG_COMMAND = 8,
} gw_message_type_t;

/* Port of Python: MessageEvent */
typedef struct {
    char *text;
    gw_message_type_t message_type;
    json_node_t *source;
    json_node_t *raw_message;
    char *message_id;
    int platform_update_id;
    char **media_urls;
    char **media_types;
    size_t media_count;
    char *reply_to_message_id;
    char *reply_to_text;
    char *auto_skill;
    char *channel_prompt;
    char *channel_context;
    bool internal;
    time_t timestamp;
} gw_message_event_t;

/* Port of Python: MessageEvent.__init__ */
gw_message_event_t *gw_message_event_new(void);
void gw_message_event_free(gw_message_event_t *event);

/* VTable function pointers (Port of Python gateway.platforms.base.BasePlatformAdapter vtable) */
typedef bool (*gw_connect_fn)(gw_base_platform_adapter_t *adapter);
typedef void (*gw_disconnect_fn)(gw_base_platform_adapter_t *adapter);
typedef gw_send_result_t (*gw_send_fn)(gw_base_platform_adapter_t *adapter,
                                        const char *chat_id, const char *content,
                                        const char *reply_to, json_node_t *metadata);
typedef gw_send_result_t (*gw_edit_message_fn)(gw_base_platform_adapter_t *adapter,
                                                const char *chat_id, const char *message_id,
                                                const char *content, bool finalize);
typedef bool (*gw_delete_message_fn)(gw_base_platform_adapter_t *adapter,
                                      const char *chat_id, const char *message_id);
typedef gw_send_result_t (*gw_send_image_fn)(gw_base_platform_adapter_t *adapter,
                                              const char *chat_id, const char *image_url,
                                              const char *caption, const char *reply_to,
                                              json_node_t *metadata);
typedef gw_send_result_t (*gw_send_animation_fn)(gw_base_platform_adapter_t *adapter,
                                                  const char *chat_id, const char *animation_url,
                                                  const char *caption, const char *reply_to,
                                                  json_node_t *metadata);
typedef gw_send_result_t (*gw_send_voice_fn)(gw_base_platform_adapter_t *adapter,
                                              const char *chat_id, const char *audio_path,
                                              const char *caption, const char *reply_to,
                                              json_node_t *metadata);
typedef gw_send_result_t (*gw_send_video_fn)(gw_base_platform_adapter_t *adapter,
                                              const char *chat_id, const char *video_path,
                                              const char *caption, const char *reply_to,
                                              json_node_t *metadata);
typedef gw_send_result_t (*gw_send_document_fn)(gw_base_platform_adapter_t *adapter,
                                                 const char *chat_id, const char *file_path,
                                                 const char *caption, const char *file_name,
                                                 const char *reply_to, json_node_t *metadata);
typedef gw_send_result_t (*gw_send_image_file_fn)(gw_base_platform_adapter_t *adapter,
                                                   const char *chat_id, const char *image_path,
                                                   const char *caption, const char *reply_to,
                                                   json_node_t *metadata);
typedef void (*gw_send_typing_fn)(gw_base_platform_adapter_t *adapter,
                                   const char *chat_id, json_node_t *metadata);
typedef void (*gw_stop_typing_fn)(gw_base_platform_adapter_t *adapter, const char *chat_id);
typedef gw_send_result_t (*gw_send_slash_confirm_fn)(gw_base_platform_adapter_t *adapter,
                                                      const char *chat_id, const char *title,
                                                      const char *message, const char *session_key,
                                                      const char *confirm_id, json_node_t *metadata);
typedef gw_send_result_t (*gw_send_clarify_fn)(gw_base_platform_adapter_t *adapter,
                                                const char *chat_id, const char *question,
                                                char **choices, size_t choices_count,
                                                const char *clarify_id, const char *session_key,
                                                json_node_t *metadata);
typedef json_node_t * (*gw_get_chat_info_fn)(gw_base_platform_adapter_t *adapter,
                                              const char *chat_id);
typedef char * (*gw_format_message_fn)(gw_base_platform_adapter_t *adapter, const char *content);

/* BasePlatformAdapter struct (Port of Python gateway.platforms.base.BasePlatformAdapter) */
struct gw_base_platform_adapter {
    /* Config & platform info */
    gw_platform_config_t *config;
    gw_platform_t platform;

    /* VTable */
    gw_connect_fn connect;
    gw_disconnect_fn disconnect;
    gw_send_fn send;
    gw_edit_message_fn edit_message;
    gw_delete_message_fn delete_message;
    gw_send_image_fn send_image;
    gw_send_animation_fn send_animation;
    gw_send_voice_fn send_voice;
    gw_send_video_fn send_video;
    gw_send_document_fn send_document;
    gw_send_image_file_fn send_image_file;
    gw_send_typing_fn send_typing;
    gw_stop_typing_fn stop_typing;
    gw_send_slash_confirm_fn send_slash_confirm;
    gw_send_clarify_fn send_clarify;
    gw_get_chat_info_fn get_chat_info;
    gw_format_message_fn format_message;

    /* Capabilities */
    bool supports_code_blocks;
    const char *typed_command_prefix;

    /* Runtime state */
    bool running;
    char *fatal_error_code;
    char *fatal_error_message;
    bool fatal_error_retryable;

    /* Session tracking */
    struct gw_session_tracker *session_tracker;  // Opaque, platform-specific

    /* Private data for subclass */
    void *private_data;
};

/* Base adapter constructor/destructor (Port of Python gateway.platforms.base.BasePlatformAdapter.__init__ / __del__) */
gw_base_platform_adapter_t *gw_base_platform_adapter_new(gw_platform_config_t *config, gw_platform_t platform);
void gw_base_platform_adapter_free(gw_base_platform_adapter_t *adapter);

/* Base adapter default implementations (Port of Python gateway.platforms.base.BasePlatformAdapter.default_*) */
bool gw_base_platform_adapter_default_connect(gw_base_platform_adapter_t *adapter);
void gw_base_platform_adapter_default_disconnect(gw_base_platform_adapter_t *adapter);
gw_send_result_t gw_base_platform_adapter_default_send(gw_base_platform_adapter_t *adapter,
                                                        const char *chat_id, const char *content,
                                                        const char *reply_to, json_node_t *metadata);
gw_send_result_t gw_base_platform_adapter_default_edit_message(gw_base_platform_adapter_t *adapter,
                                                                const char *chat_id, const char *message_id,
                                                                const char *content, bool finalize);
bool gw_base_platform_adapter_default_delete_message(gw_base_platform_adapter_t *adapter,
                                                      const char *chat_id, const char *message_id);
gw_send_result_t gw_base_platform_adapter_default_send_image(gw_base_platform_adapter_t *adapter,
                                                              const char *chat_id, const char *image_url,
                                                              const char *caption, const char *reply_to,
                                                              json_node_t *metadata);
gw_send_result_t gw_base_platform_adapter_default_send_animation(gw_base_platform_adapter_t *adapter,
                                                                  const char *chat_id, const char *animation_url,
                                                                  const char *caption, const char *reply_to,
                                                                  json_node_t *metadata);
gw_send_result_t gw_base_platform_adapter_default_send_voice(gw_base_platform_adapter_t *adapter,
                                                              const char *chat_id, const char *audio_path,
                                                              const char *caption, const char *reply_to,
                                                              json_node_t *metadata);
gw_send_result_t gw_base_platform_adapter_default_send_video(gw_base_platform_adapter_t *adapter,
                                                              const char *chat_id, const char *video_path,
                                                              const char *caption, const char *reply_to,
                                                              json_node_t *metadata);
gw_send_result_t gw_base_platform_adapter_default_send_document(gw_base_platform_adapter_t *adapter,
                                                                 const char *chat_id, const char *file_path,
                                                                 const char *caption, const char *file_name,
                                                                 const char *reply_to, json_node_t *metadata);
gw_send_result_t gw_base_platform_adapter_default_send_image_file(gw_base_platform_adapter_t *adapter,
                                                                   const char *chat_id, const char *image_path,
                                                                   const char *caption, const char *reply_to,
                                                                   json_node_t *metadata);
void gw_base_platform_adapter_default_send_typing(gw_base_platform_adapter_t *adapter,
                                                   const char *chat_id, json_node_t *metadata);
void gw_base_platform_adapter_default_stop_typing(gw_base_platform_adapter_t *adapter, const char *chat_id);
gw_send_result_t gw_base_platform_adapter_default_send_slash_confirm(gw_base_platform_adapter_t *adapter,
                                                                      const char *chat_id, const char *title,
                                                                      const char *message, const char *session_key,
                                                                      const char *confirm_id, json_node_t *metadata);
gw_send_result_t gw_base_platform_adapter_default_send_clarify(gw_base_platform_adapter_t *adapter,
                                                                const char *chat_id, const char *question,
                                                                char **choices, size_t choices_count,
                                                                const char *clarify_id, const char *session_key,
                                                                json_node_t *metadata);
json_node_t *gw_base_platform_adapter_default_get_chat_info(gw_base_platform_adapter_t *adapter,
                                                             const char *chat_id);
char *gw_base_platform_adapter_default_format_message(gw_base_platform_adapter_t *adapter,
                                                       const char *content);

/* Utility helpers (Port of Python gateway.platforms.base.BasePlatformAdapter._*) */
void gw_base_platform_adapter_mark_connected(gw_base_platform_adapter_t *adapter);
void gw_base_platform_adapter_mark_disconnected(gw_base_platform_adapter_t *adapter);
void gw_base_platform_adapter_set_fatal_error(gw_base_platform_adapter_t *adapter,
                                               const char *code, const char *message, bool retryable);
bool gw_base_platform_adapter_acquire_lock(gw_base_platform_adapter_t *adapter,
                                            const char *scope, const char *identity,
                                            const char *resource_desc);
void gw_base_platform_adapter_release_lock(gw_base_platform_adapter_t *adapter);

/* Static helpers (Port of Python gateway.platforms.base.BasePlatformAdapter.static_*) */
bool gw_should_send_media_as_audio(gw_platform_t platform, const char *ext, bool is_voice);
char *gw_prepare_tts_text(const char *text);
gw_send_result_t gw_play_tts(gw_base_platform_adapter_t *adapter,
                              const char *chat_id, const char *audio_path, json_node_t *metadata);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GATEWAY_PLATFORMS_BASE_H */