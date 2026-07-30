/*
 * gateway/platforms/base_adapter.c — BasePlatformAdapter implementation.
 *
 * Port of Python gateway/platforms/base.py BasePlatformAdapter class.
 *
 * Ports of Python (see per-function annotations):
 *   BasePlatformAdapter.__init__         -> gw_base_platform_adapter_new
 *   BasePlatformAdapter.connect          -> gw_base_platform_adapter_default_connect
 *   BasePlatformAdapter.disconnect       -> gw_base_platform_adapter_default_disconnect
 *   BasePlatformAdapter.send             -> gw_base_platform_adapter_default_send
 *   BasePlatformAdapter.edit_message     -> gw_base_platform_adapter_default_edit_message
 *   BasePlatformAdapter.delete_message   -> gw_base_platform_adapter_default_delete_message
 *   BasePlatformAdapter.send_image       -> gw_base_platform_adapter_default_send_image
 *   BasePlatformAdapter.send_animation   -> gw_base_platform_adapter_default_send_animation
 *   BasePlatformAdapter.send_voice       -> gw_base_platform_adapter_default_send_voice
 *   BasePlatformAdapter.send_video       -> gw_base_platform_adapter_default_send_video
 *   BasePlatformAdapter.send_document    -> gw_base_platform_adapter_default_send_document
 *   BasePlatformAdapter.send_image_file  -> gw_base_platform_adapter_default_send_image_file
 *   BasePlatformAdapter.send_typing      -> gw_base_platform_adapter_default_send_typing
 *   BasePlatformAdapter.stop_typing      -> gw_base_platform_adapter_default_stop_typing
 *   BasePlatformAdapter.send_slash_confirm -> gw_base_platform_adapter_default_send_slash_confirm
 *   BasePlatformAdapter.send_clarify     -> gw_base_platform_adapter_default_send_clarify
 *   BasePlatformAdapter.get_chat_info    -> gw_base_platform_adapter_default_get_chat_info
 *   BasePlatformAdapter.format_message   -> gw_base_platform_adapter_default_format_message
 *   BasePlatformAdapter._mark_connected  -> gw_base_platform_adapter_mark_connected
 *   BasePlatformAdapter._mark_disconnected -> gw_base_platform_adapter_mark_disconnected
 *   BasePlatformAdapter._set_fatal_error -> gw_base_platform_adapter_set_fatal_error
 *   BasePlatformAdapter._acquire_platform_lock -> gw_base_platform_adapter_acquire_lock
 *   BasePlatformAdapter._release_platform_lock -> gw_base_platform_adapter_release_lock
 *   BasePlatformAdapter.should_send_media_as_audio -> gw_should_send_media_as_audio
 *   BasePlatformAdapter.prepare_tts_text -> gw_prepare_tts_text
 *   BasePlatformAdapter.play_tts         -> gw_play_tts
 *   BasePlatformAdapter.build_source     -> gw_build_source
 *   BasePlatformAdapter.truncate_message -> gw_platform_truncate_message
 *   BasePlatformAdapter._is_retryable_error -> (static helper, not directly ported)
 *   BasePlatformAdapter._is_timeout_error -> (static helper, not directly ported)
 *   BasePlatformAdapter._merge_caption   -> gw_merge_caption
 *   BasePlatformAdapter._get_human_delay -> gw_get_human_delay
 *   BasePlatformAdapter._reply_anchor_for_event -> gw_reply_anchor_for_event
 *   BasePlatformAdapter._thread_metadata_for_source -> gw_thread_metadata_for_source
 */

#include "hermes_gateway_core.h"
#include "base.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_logger.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

/* ================================================================
 *  Session tracker (opaque struct for session management)
 * ================================================================ */

typedef struct gw_session_state {
    char *session_key;
    pthread_mutex_t guard_mutex;
    bool guard_active;
    pthread_t owner_thread;
    void *pending_event;  // Opaque
    struct gw_session_state *next;
} gw_session_state_t;

typedef struct gw_session_tracker {
    gw_session_state_t *sessions;
    pthread_mutex_t mutex;
} gw_session_tracker_t;

static gw_session_tracker_t *gw_session_tracker_new(void) {
    gw_session_tracker_t *tracker = malloc(sizeof(gw_session_tracker_t));
    if (!tracker) return NULL;
    tracker->sessions = NULL;
    pthread_mutex_init(&tracker->mutex, NULL);
    return tracker;
}

static void gw_session_tracker_free(gw_session_tracker_t *tracker) {
    if (!tracker) return;
    gw_session_state_t *curr = tracker->sessions;
    while (curr) {
        gw_session_state_t *next = curr->next;
        free(curr->session_key);
        pthread_mutex_destroy(&curr->guard_mutex);
        free(curr);
        curr = next;
    }
    pthread_mutex_destroy(&tracker->mutex);
    free(tracker);
}

static gw_session_state_t *gw_session_tracker_get(gw_session_tracker_t *tracker, const char *session_key) {
    if (!tracker || !session_key) return NULL;
    pthread_mutex_lock(&tracker->mutex);
    gw_session_state_t *curr = tracker->sessions;
    while (curr) {
        if (strcmp(curr->session_key, session_key) == 0) {
            pthread_mutex_unlock(&tracker->mutex);
            return curr;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&tracker->mutex);
    return NULL;
}

static gw_session_state_t *gw_session_tracker_ensure(gw_session_tracker_t *tracker, const char *session_key) {
    if (!tracker || !session_key) return NULL;
    pthread_mutex_lock(&tracker->mutex);
    gw_session_state_t *curr = tracker->sessions;
    while (curr) {
        if (strcmp(curr->session_key, session_key) == 0) {
            pthread_mutex_unlock(&tracker->mutex);
            return curr;
        }
        curr = curr->next;
    }
    curr = malloc(sizeof(gw_session_state_t));
    if (curr) {
        curr->session_key = strdup(session_key);
        pthread_mutex_init(&curr->guard_mutex, NULL);
        curr->guard_active = false;
        curr->owner_thread = 0;
        curr->pending_event = NULL;
        curr->next = tracker->sessions;
        tracker->sessions = curr;
    }
    pthread_mutex_unlock(&tracker->mutex);
    return curr;
}

/* ================================================================
 *  BasePlatformAdapter implementation
 * ================================================================ */

/* Port of Python: __init__ */
/* Base adapter constructor */
gw_base_platform_adapter_t *gw_base_platform_adapter_new(gw_platform_config_t *config, gw_platform_t platform) {
    gw_base_platform_adapter_t *adapter = malloc(sizeof(gw_base_platform_adapter_t));
    if (!adapter) return NULL;

    adapter->config = config;
    adapter->platform = platform;

    /* Default vtable */
    adapter->connect = gw_base_platform_adapter_default_connect;
    adapter->disconnect = gw_base_platform_adapter_default_disconnect;
    adapter->send = gw_base_platform_adapter_default_send;
    adapter->edit_message = gw_base_platform_adapter_default_edit_message;
    adapter->delete_message = gw_base_platform_adapter_default_delete_message;
    adapter->send_image = gw_base_platform_adapter_default_send_image;
    adapter->send_animation = gw_base_platform_adapter_default_send_animation;
    adapter->send_voice = gw_base_platform_adapter_default_send_voice;
    adapter->send_video = gw_base_platform_adapter_default_send_video;
    adapter->send_document = gw_base_platform_adapter_default_send_document;
    adapter->send_image_file = gw_base_platform_adapter_default_send_image_file;
    adapter->send_typing = gw_base_platform_adapter_default_send_typing;
    adapter->stop_typing = gw_base_platform_adapter_default_stop_typing;
    adapter->send_slash_confirm = gw_base_platform_adapter_default_send_slash_confirm;
    adapter->send_clarify = gw_base_platform_adapter_default_send_clarify;
    adapter->get_chat_info = gw_base_platform_adapter_default_get_chat_info;
    adapter->format_message = gw_base_platform_adapter_default_format_message;

    /* Capabilities */
    adapter->supports_code_blocks = false;
    adapter->typed_command_prefix = "/";

    /* Runtime state */
    adapter->running = false;
    adapter->fatal_error_code = NULL;
    adapter->fatal_error_message = NULL;
    adapter->fatal_error_retryable = true;

    /* Session tracking */
    adapter->session_tracker = gw_session_tracker_new();

    /* Private data */
    adapter->private_data = NULL;

    return adapter;
}

void gw_base_platform_adapter_free(gw_base_platform_adapter_t *adapter) {
    if (!adapter) return;
    free(adapter->fatal_error_code);
    free(adapter->fatal_error_message);
    gw_session_tracker_free(adapter->session_tracker);
    free(adapter);
}

/* Default implementations */

/* Port of Python: connect */
bool gw_base_platform_adapter_default_connect(gw_base_platform_adapter_t *adapter) {
    (void)adapter;
    return false;  // Subclasses must override
}

/* Port of Python: disconnect */
void gw_base_platform_adapter_default_disconnect(gw_base_platform_adapter_t *adapter) {
    (void)adapter;
    // Subclasses must override
}

/* Port of Python: send */
gw_send_result_t gw_base_platform_adapter_default_send(gw_base_platform_adapter_t *adapter,
                                                        const char *chat_id, const char *content,
                                                        const char *reply_to, json_node_t *metadata) {
    (void)adapter; (void)chat_id; (void)content; (void)reply_to; (void)metadata;
    return gw_send_result_new(false, NULL, "Not supported", NULL, false);
}

/* Port of Python: edit_message */
gw_send_result_t gw_base_platform_adapter_default_edit_message(gw_base_platform_adapter_t *adapter,
                                                                const char *chat_id, const char *message_id,
                                                                const char *content, bool finalize) {
    (void)adapter; (void)chat_id; (void)message_id; (void)content; (void)finalize;
    return gw_send_result_new(false, NULL, "Not supported", NULL, false);
}

/* Port of Python: delete_message */
bool gw_base_platform_adapter_default_delete_message(gw_base_platform_adapter_t *adapter,
                                                      const char *chat_id, const char *message_id) {
    (void)adapter; (void)chat_id; (void)message_id;
    return false;
}

/* Port of Python: send_image */
gw_send_result_t gw_base_platform_adapter_default_send_image(gw_base_platform_adapter_t *adapter,
                                                              const char *chat_id, const char *image_url,
                                                              const char *caption, const char *reply_to,
                                                              json_node_t *metadata) {
    (void)adapter; (void)chat_id; (void)image_url; (void)caption; (void)reply_to; (void)metadata;
    return gw_base_platform_adapter_default_send(adapter, chat_id, caption ? caption : image_url, reply_to, metadata);
}

/* Port of Python: send_animation */
gw_send_result_t gw_base_platform_adapter_default_send_animation(gw_base_platform_adapter_t *adapter,
                                                                  const char *chat_id, const char *animation_url,
                                                                  const char *caption, const char *reply_to,
                                                                  json_node_t *metadata) {
    return gw_base_platform_adapter_default_send_image(adapter, chat_id, animation_url, caption, reply_to, metadata);
}

/* Port of Python: send_voice */
gw_send_result_t gw_base_platform_adapter_default_send_voice(gw_base_platform_adapter_t *adapter,
                                                              const char *chat_id, const char *audio_path,
                                                              const char *caption, const char *reply_to,
                                                              json_node_t *metadata) {
    char *text = malloc(strlen("🔊 Audio: ") + strlen(audio_path) + 1);
    sprintf(text, "🔊 Audio: %s", audio_path);
    gw_send_result_t result = gw_base_platform_adapter_default_send(adapter, chat_id, text, reply_to, metadata);
    free(text);
    return result;
}

/* Port of Python: send_video */
gw_send_result_t gw_base_platform_adapter_default_send_video(gw_base_platform_adapter_t *adapter,
                                                              const char *chat_id, const char *video_path,
                                                              const char *caption, const char *reply_to,
                                                              json_node_t *metadata) {
    char *text = malloc(strlen("🎬 Video: ") + strlen(video_path) + 1);
    sprintf(text, "🎬 Video: %s", video_path);
    gw_send_result_t result = gw_base_platform_adapter_default_send(adapter, chat_id, text, reply_to, metadata);
    free(text);
    return result;
}

/* Port of Python: send_document */
gw_send_result_t gw_base_platform_adapter_default_send_document(gw_base_platform_adapter_t *adapter,
                                                                 const char *chat_id, const char *file_path,
                                                                 const char *caption, const char *file_name,
                                                                 const char *reply_to, json_node_t *metadata) {
    (void)file_name;
    char *text = malloc(strlen("📎 File: ") + strlen(file_path) + 1);
    sprintf(text, "📎 File: %s", file_path);
    gw_send_result_t result = gw_base_platform_adapter_default_send(adapter, chat_id, text, reply_to, metadata);
    free(text);
    return result;
}

/* Port of Python: send_image_file */
gw_send_result_t gw_base_platform_adapter_default_send_image_file(gw_base_platform_adapter_t *adapter,
                                                                   const char *chat_id, const char *image_path,
                                                                   const char *caption, const char *reply_to,
                                                                   json_node_t *metadata) {
    char *text = malloc(strlen("🖼️ Image: ") + strlen(image_path) + 1);
    sprintf(text, "🖼️ Image: %s", image_path);
    gw_send_result_t result = gw_base_platform_adapter_default_send(adapter, chat_id, text, reply_to, metadata);
    free(text);
    return result;
}

/* Port of Python: send_typing */
void gw_base_platform_adapter_default_send_typing(gw_base_platform_adapter_t *adapter,
                                                   const char *chat_id, json_node_t *metadata) {
    (void)adapter; (void)chat_id; (void)metadata;
    // Optional - subclasses override if platform supports it
}

/* Port of Python: stop_typing */
void gw_base_platform_adapter_default_stop_typing(gw_base_platform_adapter_t *adapter, const char *chat_id) {
    (void)adapter; (void)chat_id;
    // Optional - subclasses override if platform supports it
}

/* Port of Python: send_slash_confirm */
gw_send_result_t gw_base_platform_adapter_default_send_slash_confirm(gw_base_platform_adapter_t *adapter,
                                                                      const char *chat_id, const char *title,
                                                                      const char *message, const char *session_key,
                                                                      const char *confirm_id, json_node_t *metadata) {
    (void)adapter; (void)chat_id; (void)title; (void)message; (void)session_key; (void)confirm_id; (void)metadata;
    return gw_send_result_new(false, NULL, "Not supported", NULL, false);
}

/* Port of Python: send_clarify */
gw_send_result_t gw_base_platform_adapter_default_send_clarify(gw_base_platform_adapter_t *adapter,
                                                                const char *chat_id, const char *question,
                                                                char **choices, size_t choices_count,
                                                                const char *clarify_id, const char *session_key,
                                                                json_node_t *metadata) {
    (void)adapter; (void)chat_id; (void)question; (void)choices; (void)choices_count;
    (void)clarify_id; (void)session_key; (void)metadata;
    
    /* Text fallback - build numbered list */
    char *text = malloc(1024);
    if (!text) return gw_send_result_new(false, NULL, "OOM", NULL, false);
    
    if (choices && choices_count > 0) {
        sprintf(text, "❓ %s\n\n", question);
        for (size_t i = 0; i < choices_count; i++) {
            strcat(text, "  ");
            char num[16];
            sprintf(num, "%zu. ", i + 1);
            strcat(text, num);
            strcat(text, choices[i]);
            strcat(text, "\n");
        }
        strcat(text, "\nReply with the number, the option text, or your own answer.");
    } else {
        sprintf(text, "❓ %s", question);
    }
    
    gw_send_result_t result = gw_base_platform_adapter_default_send(adapter, chat_id, text, NULL, metadata);
    free(text);
    return result;
}

/* Port of Python: get_chat_info */
json_node_t *gw_base_platform_adapter_default_get_chat_info(gw_base_platform_adapter_t *adapter,
                                                             const char *chat_id) {
    (void)adapter; (void)chat_id;
    return NULL;
}

/* Port of Python: format_message */
char *gw_base_platform_adapter_default_format_message(gw_base_platform_adapter_t *adapter,
                                                       const char *content) {
    (void)adapter;
    return content ? strdup(content) : strdup("");
}

/* Utility helpers */

/* Port of Python: _mark_connected */
void gw_base_platform_adapter_mark_connected(gw_base_platform_adapter_t *adapter) {
    if (!adapter) return;
    adapter->running = true;
    free(adapter->fatal_error_code);
    adapter->fatal_error_code = NULL;
    free(adapter->fatal_error_message);
    adapter->fatal_error_message = NULL;
    adapter->fatal_error_retryable = true;
    // Write runtime status
}

/* Port of Python: _mark_disconnected */
void gw_base_platform_adapter_mark_disconnected(gw_base_platform_adapter_t *adapter) {
    if (!adapter) return;
    adapter->running = false;
    if (adapter->fatal_error_message) return;  // Don't overwrite fatal error
    // Write runtime status
}

/* Port of Python: _set_fatal_error */
void gw_base_platform_adapter_set_fatal_error(gw_base_platform_adapter_t *adapter,
                                               const char *code, const char *message, bool retryable) {
    if (!adapter) return;
    adapter->running = false;
    free(adapter->fatal_error_code);
    adapter->fatal_error_code = code ? strdup(code) : NULL;
    free(adapter->fatal_error_message);
    adapter->fatal_error_message = message ? strdup(message) : NULL;
    adapter->fatal_error_retryable = retryable;
    // Write runtime status
}

/* Port of Python: _acquire_platform_lock */
bool gw_base_platform_adapter_acquire_lock(gw_base_platform_adapter_t *adapter,
                                            const char *scope, const char *identity,
                                            const char *resource_desc) {
    (void)adapter;
    // Use gateway status module
    return false;  // Placeholder - would call acquire_scoped_lock
}

/* Port of Python: _release_platform_lock */
void gw_base_platform_adapter_release_lock(gw_base_platform_adapter_t *adapter) {
    (void)adapter;
    // Use gateway status module
}

/* Port of Python: should_send_media_as_audio */
/* Static helpers (from Python BasePlatformAdapter) */

bool gw_should_send_media_as_audio(gw_platform_t platform, const char *ext, bool is_voice) {
    (void)platform; // Platform-specific logic can be added when adapters are fully ported
    if (!ext) return false;
    
    // _AUDIO_EXTS
    const char *audio_exts[] = {".ogg", ".opus", ".mp3", ".wav", ".m4a", ".flac"};
    bool is_audio = false;
    for (size_t i = 0; i < sizeof(audio_exts)/sizeof(audio_exts[0]); i++) {
        if (strcasecmp(ext, audio_exts[i]) == 0) {
            is_audio = true;
            break;
        }
    }
    if (!is_audio) return false;
    
    // For all platforms except Telegram behavior, treat all audio same
    // Telegram-specific logic can be added when platform adapters are fully ported
    return true;
}

/* Port of Python: prepare_tts_text */
char *gw_prepare_tts_text(const char *text) {
    if (!text) return strdup("");
    
    // Strip markdown formatting
    char *result = strdup(text);
    if (!result) return NULL;
    
    // Remove *, _, `, #, [, ], (, )
    char *p = result;
    char *q = result;
    while (*p) {
        if (*p != '*' && *p != '_' && *p != '`' && *p != '#' &&
            *p != '[' && *p != ']' && *p != '(' && *p != ')') {
            *q++ = *p;
        }
        p++;
    }
    *q = '\0';
    
    // Truncate to 4000 chars
    if (strlen(result) > 4000) {
        result[4000] = '\0';
    }
    
    return result;
}

/* Port of Python: play_tts */
gw_send_result_t gw_play_tts(gw_base_platform_adapter_t *adapter,
                              const char *chat_id, const char *audio_path, json_node_t *metadata) {
    return gw_base_platform_adapter_default_send_voice(adapter, chat_id, audio_path, NULL, NULL, metadata);
}

/* SendResult helpers */

gw_send_result_t gw_send_result_new(bool success, const char *message_id, const char *error,
                                     json_node_t *raw_response, bool retryable) {
    gw_send_result_t result = {0};
    result.success = success;
    result.message_id = message_id ? strdup(message_id) : NULL;
    result.error = error ? strdup(error) : NULL;
    result.raw_response = raw_response;
    result.retryable = retryable;
    result.continuation_message_ids = NULL;
    result.continuation_count = 0;
    return result;
}

void gw_send_result_free(gw_send_result_t *result) {
    if (!result) return;
    free(result->message_id);
    free(result->error);
    if (result->raw_response) json_free(result->raw_response);
    for (size_t i = 0; i < result->continuation_count; i++) {
        free(result->continuation_message_ids[i]);
    }
    free(result->continuation_message_ids);
}

/* MessageEvent helpers */

gw_message_event_t *gw_message_event_new(void) {
    return calloc(1, sizeof(gw_message_event_t));
}

void gw_message_event_free(gw_message_event_t *event) {
    if (!event) return;
    free(event->text);
    if (event->source) json_free(event->source);
    if (event->raw_message) json_free(event->raw_message);
    free(event->message_id);
    for (size_t i = 0; i < event->media_count; i++) {
        free(event->media_urls[i]);
        free(event->media_types[i]);
    }
    free(event->media_urls);
    free(event->media_types);
    free(event->reply_to_message_id);
    free(event->reply_to_text);
    free(event->auto_skill);
    free(event->channel_prompt);
    free(event->channel_context);
    free(event);
}

/* Session/typing helpers */

/* Port of Python: _get_human_delay */
double gw_get_human_delay(void) {
    char *mode = getenv("HERMES_HUMAN_DELAY_MODE");
    if (!mode || strcmp(mode, "off") == 0) return 0.0;
    
    if (strcmp(mode, "natural") == 0) {
        return 0.8 + (rand() / (double)RAND_MAX) * 1.7;  // 0.8-2.5s
    }
    
    int min_ms = 800, max_ms = 2500;
    char *min_env = getenv("HERMES_HUMAN_DELAY_MIN_MS");
    if (min_env) min_ms = atoi(min_env);
    char *max_env = getenv("HERMES_HUMAN_DELAY_MAX_MS");
    if (max_env) max_ms = atoi(max_env);
    
    return (min_ms + (rand() % (max_ms - min_ms + 1))) / 1000.0;
}

/* Port of Python: _reply_anchor_for_event */
char *gw_reply_anchor_for_event(const json_node_t *event_source, const char *reply_to_message_id) {
    if (!event_source) return reply_to_message_id ? strdup(reply_to_message_id) : NULL;
    
    // Extract platform and thread info from source
    // Simplified - would need full implementation
    return reply_to_message_id ? strdup(reply_to_message_id) : NULL;
}

/* Port of Python: _thread_metadata_for_source */
json_node_t *gw_thread_metadata_for_source(const json_node_t *source, const char *reply_to_message_id) {
    if (!source) return NULL;
    
    json_node_t *obj = json_object();
    if (!obj) return NULL;
    
    // Extract thread_id from source
    // Simplified
    return obj;
}
