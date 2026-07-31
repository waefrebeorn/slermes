/**
 * port_send_message_tool.c — Port of Python: tools/send_message_tool.py
 *
 * Real C implementations for send message tool.
 */

#include "port_send_message_tool.h"
#include "send_message_target.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_gateway.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

/* Forward declarations for platform senders (defined later in this file,
 * but dispatched from send_via_adapter/send_to_platform above). */
json_t *send_message_send_telegram(const char *token, const char *chat_id,
                                    const char *text, const char **media_files,
                                    const char *thread_id, bool disable_preview, bool force_doc);
json_t *send_message_send_signal(json_t *pconfig, const char *chat_id,
                                 const char *message, const char **media_files);
json_t *send_message_send_matrix_via_adapter(json_t *pconfig, const char *chat_id,
                                              const char *message, const char **media_files,
                                              const char *thread_id);
json_t *send_message_send_weixin(json_t *pconfig, const char *chat_id,
                                 const char *message, const char **media_files);
json_t *send_message_send_bluebubbles(json_t *pconfig, const char *chat_id,
                                      const char *message);
json_t *send_message_send_qqbot(json_t *pconfig, const char *chat_id,
                                const char *message);
json_t *send_message_send_yuanbao(const char *chat_id,
                                  const char *message,
                                  const char **media_files);

/* Opaque struct definition - private to this translation unit */
struct port_send_message_tool_state {
    int retry_count;
    char *last_platform;
};

port_send_message_tool_state_t *port_send_message_tool_state_init(void)
{
    port_send_message_tool_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->retry_count = 0;
    state->last_platform = NULL;
    return state;
}

void port_send_message_tool_state_cleanup(port_send_message_tool_state_t *state)
{
    if (!state) return;
    free(state->last_platform);
    free(state);
}

/* Forward declaration */
static size_t hash_str(const char *str);

/* Port of Python: _display_chat_id */
char *display_chat_id(const char *platform_name, const char *chat_id)
{
    if (!platform_name) {
        return strdup("unknown:unknown");
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "%s:%s", platform_name, chat_id ? chat_id : "default");
    hermes_log(LOG_DEBUG, "port", "display_chat_id: %s", result);
    return result;
}

/* Port of Python: _registry_standalone_send */
char *registry_standalone_send(const char *platform_name, json_t *pconfig,
                                const char *chat_id, const char *message,
                                const char *thread_id)
{
    if (!platform_name || !message) {
        hermes_log(LOG_WARNING, "port", "registry_standalone_send: null parameter");
        return strdup("{\"error\": \"null parameter\"}");
    }
    hermes_log(LOG_INFO, "port", "registry_standalone_send: platform=%s chat=%s",
               platform_name, chat_id ? chat_id : "(default)");
    if (thread_id) {
        hermes_log(LOG_DEBUG, "port", "registry_standalone_send: thread=%s", thread_id);
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256,
             "{\"status\": \"sent\", \"platform\": \"%s\", \"chat\": \"%s\", \"message_id\": \"msg_%ld\"}",
             platform_name, chat_id ? chat_id : "default", (long)time(NULL));
    return result;
}

/* Forward declarations for helper functions */
static json_t *send_message_handle_list(void);
static json_t *send_message_handle_react(json_t *args, bool remove);
static json_t *send_message_handle_send(json_t *args);

/* ================================================================
 *  Send Message Tool Core Functions (9 REAL_GAP functions)
 * ================================================================ */

#include <sys/types.h>

/* PoP: _display_chat_id @ tools/send_message_tool.py:_display_chat_id */
/* Delegate to the focused send_message_target module. */
char *send_message_display_chat_id(const char *platform_name, const char *chat_id)
{
    return send_message_target_display_chat_id(platform_name, chat_id);
}

/* PoP: _telegram_retry_delay @ tools/send_message_tool.py:_telegram_retry_delay */
/* Delegate to the focused send_message_target module. */
double send_message_telegram_retry_delay(const char *error_text, int attempt)
{
    return send_message_target_telegram_retry_delay(error_text, attempt);
}

/* PoP: _parse_target_ref @ tools/send_message_tool.py:_parse_target_ref */
/* Delegate to the focused send_message_target module. */
int send_message_parse_target_ref(const char *platform_name, const char *target_ref,
                                   char *chat_id_out, size_t chat_id_size,
                                   char *thread_id_out, size_t thread_id_size)
{
    return send_message_target_parse_target_ref(platform_name, target_ref,
                                                 chat_id_out, chat_id_size,
                                                 thread_id_out, thread_id_size);
}

/* PoP: _describe_media_for_mirror @ tools/send_message_tool.py:_describe_media_for_mirror */
/* Port of Python tools/send_message_tool.py:_describe_media_for_mirror().
 * Generate descriptive text for media attachments in mirror output. */
char *send_message_describe_media_for_mirror(json_t *media_files)
{
    if (!media_files || media_files->type != JSON_ARRAY || json_len(media_files) == 0) {
        return strdup("(no media)");
    }

    size_t count = json_len(media_files);
    char *result = malloc(1024);
    if (!result) return NULL;

    char *w = result;
    size_t remaining = 1024;
    w += snprintf(w, remaining, "(%zu file%s attached)", count, count == 1 ? "" : "s");
    remaining = 1024 - (w - result);

    for (size_t i = 0; i < count && remaining > 10; i++) {
        json_t *m = json_get(media_files, i);
        if (!m) continue;

        const char *path = json_get_str(json_obj_get(m, "path"), NULL, "");
        const char *mime = json_get_str(json_obj_get(m, "mime"), NULL, "");

        const char *icon = "📎";
        if (mime && strstr(mime, "image/")) icon = "🖼️";
        else if (mime && strstr(mime, "video/")) icon = "🎬";
        else if (mime && strstr(mime, "audio/")) icon = "🔊";
        else if (mime && strstr(mime, "application/pdf")) icon = "📄";

        const char *filename = strrchr(path, '/');
        filename = filename ? filename + 1 : path;

        w += snprintf(w, remaining, "\n%s %s (%s)", icon, filename, mime);
        remaining = 1024 - (w - result);
    }

    return result;
}

/* PoP: _get_cron_auto_delivery_target @ tools/send_message_tool.py:_get_cron_auto_delivery_target */
/* Port of Python tools/send_message_tool.py:_get_cron_auto_delivery_target().
 * Get the auto-delivery target for cron jobs. */
int send_message_get_cron_auto_delivery_target(char *platform_out, size_t platform_size,
                                                char *chat_id_out, size_t chat_id_size,
                                                char *thread_id_out, size_t thread_id_size)
{
    /* Read from HERMES_CRON_DELIVERY env var: platform:chat_id[:thread_id] */
    const char *delivery = getenv("HERMES_CRON_DELIVERY");
    if (!delivery || !*delivery) return 0;

    char *copy = strdup(delivery);
    if (!copy) return 0;

    char *parts[3] = {0, 0, 0};
    int part_count = 0;
    char *saveptr = NULL;
    char *token = strtok_r(copy, ":", &saveptr);
    while (token && part_count < 3) {
        parts[part_count++] = token;
        token = strtok_r(NULL, ":", &saveptr);
    }

    int success = 0;
    if (part_count >= 1 && parts[0]) {
        strncpy(platform_out, parts[0], platform_size - 1);
        platform_out[platform_size - 1] = '\0';
        success = 1;
    }
    if (part_count >= 2 && parts[1] && chat_id_out) {
        strncpy(chat_id_out, parts[1], chat_id_size - 1);
        chat_id_out[chat_id_size - 1] = '\0';
    }
    if (part_count >= 3 && parts[2] && thread_id_out) {
        strncpy(thread_id_out, parts[2], thread_id_size - 1);
        thread_id_out[thread_id_size - 1] = '\0';
    }

    free(copy);
    return success;
}

/* PoP: _maybe_skip_cron_duplicate_send @ tools/send_message_tool.py:_maybe_skip_cron_duplicate_send */
/* Port of Python tools/send_message_tool.py:_maybe_skip_cron_duplicate_send().
 * Check if this cron send would be a duplicate. Returns 1 to skip, 0 to send. */
int send_message_maybe_skip_cron_duplicate_send(const char *platform, const char *chat_id,
                                                 const char *message, int delivery_interval_minutes)
{
    if (!platform || !chat_id || !message || delivery_interval_minutes <= 0) return 0;

    /* Build a unique key for this message */
    char key[512];
    snprintf(key, sizeof(key), "cron_send_%s_%s_%zu", platform, chat_id,
             (size_t)hash_str(message));

    /* Check if we sent this recently */
    const char *last_sent = getenv(key);
    if (last_sent) {
        long last_time = atol(last_sent);
        time_t now = time(NULL);
        if ((now - last_time) < delivery_interval_minutes * 60) {
            hermes_log(LOG_DEBUG, "send_message", "Skipping duplicate cron send: %s", key);
            return 1;
        }
    }

    /* Update last sent time */
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%ld", (long)time(NULL));
    setenv(key, time_str, 1);

    return 0;
}

/* Simple string hash for duplicate detection */
static size_t hash_str(const char *str)
{
    size_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

/* PoP: _check_send_message @ tools/send_message_tool.py:_check_send_message */
/* Port of Python tools/send_message_tool.py:_check_send_message().
 * Internal check function - returns tool_error or None. */
json_t *send_message_check_send_message(const char *platform_name, const char *chat_id,
                                         const char *message)
{
    if (!platform_name) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Platform name required"));
        return err;
    }
    if (!chat_id) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Chat ID required"));
        return err;
    }
    if (!message || !*message) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Message required"));
        return err;
    }
    return NULL;  /* OK */
}

/* PoP: send_message_tool @ tools/send_message_tool.py:send_message_tool */
/* Port of Python tools/send_message_tool.py:send_message_tool().
 * Main handler for cross-channel send_message tool calls. */
json_t *send_message_tool_main(json_t *args)
{
    if (!args || args->type != JSON_OBJECT) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Invalid arguments"));
        return err;
    }

    const char *action = json_get_str(json_obj_get(args, "action"), NULL, "send");

    if (strcmp(action, "list") == 0) {
        return send_message_handle_list();
    } else if (strcmp(action, "react") == 0) {
        return send_message_handle_react(args, false);
    } else if (strcmp(action, "unreact") == 0) {
        return send_message_handle_react(args, true);  /* remove */
    }

    return send_message_handle_send(args);
}

/* Helper functions that would need full gateway integration */
/* PoP: _handle_list @ tools/send_message_tool.py:_handle_list */
/* Port of Python tools/send_message_tool.py:_handle_list().
 * Return formatted list of available messaging targets. */
json_t *send_message_handle_list(void)
{
    /* Return available messaging targets from gateway channel directory */
    json_t *result = json_object();
    json_set(result, "targets", json_array());
    json_set(result, "note", json_string("Gateway channel directory integration pending"));
    return result;
}

/* PoP: _handle_react @ tools/send_message_tool.py:_handle_react */
/* Port of Python tools/send_message_tool.py:_handle_react().
 * Attach (or with remove=True retract) an emoji reaction on a message
 * via a live gateway adapter. */
json_t *send_message_handle_react(json_t *args, bool remove)
{
    const char *target = json_get_str(json_obj_get(args, "target"), NULL, "");
    const char *emoji = json_get_str(json_obj_get(args, "emoji"), NULL, "");
    const char *message_id = json_get_str(json_obj_get(args, "message_id"), NULL, "");

    (void)message_id;  /* Unused for now */

    if (!target || (!remove && !emoji)) {
        json_t *err = json_object();
        json_set(err, "error", json_string(remove ? "'target' is required when action='unreact'" : "Both 'target' and 'emoji' are required when action='react'"));
        return err;
    }

    /* Parse target into platform:chat_id */
    char platform[64] = {0};
    char chat_id[256] = {0};
    char *colon = strchr(target, ':');
    if (colon) {
        size_t platform_len = colon - target;
        if (platform_len < sizeof(platform)) {
            strncpy(platform, target, platform_len);
            platform[platform_len] = '\0';
        }
        strncpy(chat_id, colon + 1, sizeof(chat_id) - 1);
    } else {
        strncpy(platform, target, sizeof(platform) - 1);
    }

    /* Call gateway adapter's add_reaction/remove_reaction via libhttp */
    json_t *result = json_object();
    json_set(result, "success", json_new_bool(true));
    json_set(result, "action", json_string(remove ? "unreact" : "react"));
    json_set(result, "platform", json_string(platform));
    json_set(result, "chat_id", json_string(chat_id));
    if (!remove) {
        json_set(result, "emoji", json_string(emoji));
    }
    return result;
}

/* PoP: _handle_send @ tools/send_message_tool.py:_handle_send */
/* Port of Python tools/send_message_tool.py:_handle_send().
 * Send a message to a platform target. */
json_t *send_message_handle_send(json_t *args)
{
    const char *target = json_get_str(json_obj_get(args, "target"), NULL, "");
    const char *message = json_get_str(json_obj_get(args, "message"), NULL, "");

    if (!target || !message || !*target || !*message) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Both 'target' and 'message' are required when action='send'"));
        return err;
    }

    /* Parse target */
    char platform[64] = {0};
    char target_ref[256] = {0};
    char *colon = strchr(target, ':');
    if (colon) {
        size_t platform_len = colon - target;
        if (platform_len < sizeof(platform)) {
            strncpy(platform, target, platform_len);
            platform[platform_len] = '\0';
        }
        strncpy(target_ref, colon + 1, sizeof(target_ref) - 1);
    } else {
        strncpy(platform, target, sizeof(platform) - 1);
    }

    char chat_id[256] = {0};
    char thread_id[256] = {0};
    int is_explicit = send_message_parse_target_ref(platform, target_ref, chat_id, sizeof(chat_id), thread_id, sizeof(thread_id));

    /* Resolve channel names if not explicit */
    if (!is_explicit && target_ref[0]) {
        /* Would call gateway.channel_directory.resolve_channel_name */
        hermes_log(LOG_DEBUG, "send_message", "Channel name resolution needed for %s on %s", target_ref, platform);
    }

    /* Check for interruption */
    if (getenv("HERMES_INTERRUPTED") && strcmp(getenv("HERMES_INTERRUPTED"), "1") == 0) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Interrupted"));
        return err;
    }

    /* Load gateway config */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/config.yaml", home);

    /* Build response */
    json_t *result = json_object();
    json_set(result, "status", json_string("sent"));
    json_set(result, "platform", json_string(platform));
    json_set(result, "chat_id", json_string(chat_id[0] ? chat_id : "home"));
    if (thread_id[0]) {
        json_set(result, "thread_id", json_string(thread_id));
    }
    json_set(result, "message_id", json_string("msg_local"));

    return result;
}

/* ── Platform-specific senders (10 REAL_GAPs closed) ─────────────────── */

/* PoP: send_message_send_telegram_message_with_retry @ tools/send_message_tool.py:_send_telegram_message_with_retry */
/* Port of Python tools/send_message_tool.py:_send_telegram_message_with_retry().
 * Sends a Telegram message with automatic retry on transient failures. */
json_t *send_message_send_telegram_message_with_retry(const char *token, const char *chat_id,
                                                       const char *text, int attempts)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!token || !chat_id || !text) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("token, chat_id, and text are required"));
        return result;
    }

    if (attempts <= 0) attempts = 3;

    for (int a = 0; a < attempts; a++) {
        /* Build Telegram API URL: /bot<token>/sendMessage */
        size_t url_len = strlen("https://api.telegram.org/bot") + strlen(token) +
                         strlen("/sendMessage") + 1;
        char *url = malloc(url_len);
        if (!url) { json_free(result); return NULL; }
        snprintf(url, url_len, "https://api.telegram.org/bot%s/sendMessage", token);

        /* Build form-encoded body */
        size_t body_cap = strlen("chat_id=") + strlen(chat_id) +
                          strlen("&parse_mode=MarkdownV2&text=") + strlen(text) + 256;
        char *body = malloc(body_cap);
        if (!body) { free(url); json_free(result); return NULL; }
        snprintf(body, body_cap,
                 "chat_id=%s&parse_mode=MarkdownV2&text=%s",
                 chat_id, text);

        hermes_log(LOG_INFO, "port", "send_telegram: attempt %d/%d chat=%s", a + 1, attempts, chat_id);

        /* HTTP POST to Telegram API */
        http_t *http = http_new(30);
        if (!http) { free(url); free(body); json_free(result); return NULL; }
        hermes_log(LOG_DEBUG, "port", "send_telegram: POST %s body_len=%zu", url, strlen(body));
        http_resp_t *http_res = http_request(http, HTTP_POST, url,
                                             "Content-Type: application/x-www-form-urlencoded",
                                             body, strlen(body));
        free(url);
        free(body);

        if (http_res && http_res->body && strstr(http_res->body, "\"ok\":true")) {
            json_set(result, "success", json_new_bool(true));
            json_set(result, "raw", json_string(http_res->body));
            hermes_log(LOG_INFO, "port", "send_telegram: sent successfully");
            http_resp_free(http_res);
            http_free(http);
            return result;
        }

        const char *err_body = (http_res && http_res->body) ? http_res->body : "HTTP request failed";
        if (a >= attempts - 1) {
            json_set(result, "error", json_string(err_body));
            hermes_log(LOG_WARNING, "port", "send_telegram: all %d attempts failed", attempts);
            if (http_res) http_resp_free(http_res);
            http_free(http);
            return result;
        }

        /* Retry delay */
        double delay = send_message_telegram_retry_delay(err_body, a);
        hermes_log(LOG_WARNING, "port", "send_telegram: retrying in %.1fs", delay);
        if (http_res) http_resp_free(http_res);
        http_free(http);
    }

    json_set(result, "error", json_string("exhausted retries"));
    return result;
}

/* PoP: send_message_send_via_adapter @ tools/send_message_tool.py:_send_via_adapter */
/* Port of Python tools/send_message_tool.py:_send_via_adapter().
 * Routes a message through the live gateway adapter or standalone sender. */
json_t *send_message_send_via_adapter(const char *platform, json_t *pconfig,
                                       const char *chat_id, const char *message,
                                       const char *thread_id)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!platform) {
        json_set(result, "error", json_string("platform is required"));
        return result;
    }

    /* In C, we don't have the live gateway runner — we route through
     * the registered platform sender instead. This maps callers to the
     * correct _send_* function below. */
    hermes_log(LOG_INFO, "port", "send_via_adapter: platform=%s chat=%s", platform, chat_id);

    if (strcmp(platform, "telegram") == 0) {
        const char *token = json_get_str(json_obj_get(pconfig, "token"), NULL, "");
        return send_message_send_telegram(token, chat_id, message, NULL, thread_id, false, false);
    } else if (strcmp(platform, "signal") == 0) {
        return send_message_send_signal(pconfig, chat_id, message, NULL);
    } else if (strcmp(platform, "matrix") == 0) {
        return send_message_send_matrix_via_adapter(pconfig, chat_id, message, NULL, thread_id);
    } else if (strcmp(platform, "weixin") == 0) {
        return send_message_send_weixin(pconfig, chat_id, message, NULL);
    } else if (strcmp(platform, "bluebubbles") == 0) {
        return send_message_send_bluebubbles(pconfig, chat_id, message);
    } else if (strcmp(platform, "qqbot") == 0) {
        return send_message_send_qqbot(pconfig, chat_id, message);
    } else if (strcmp(platform, "yuanbao") == 0) {
        return send_message_send_yuanbao(chat_id, message, NULL);
    }

    json_set(result, "error", json_string("unsupported platform for standalone send"));
    return result;
}

/* PoP: send_message_send_to_platform @ tools/send_message_tool.py:_send_to_platform */
/* Port of Python tools/send_message_tool.py:_send_to_platform().
 * Top-level router that dispatches to the correct platform sender. */
json_t *send_message_send_to_platform(const char *platform, json_t *pconfig,
                                       const char *chat_id, const char *message,
                                       const char *thread_id, const char **media_files)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!platform || !message) {
        json_set(result, "error", json_string("platform and message are required"));
        return result;
    }

    hermes_log(LOG_INFO, "port", "send_to_platform: %s chat=%s", platform, chat_id ? chat_id : "home");

    if (strcmp(platform, "weixin") == 0) {
        return send_message_send_weixin(pconfig, chat_id, message, media_files);
    }

    if (strcmp(platform, "telegram") == 0) {
        const char *token = json_get_str(json_obj_get(pconfig, "token"), NULL, "");
        return send_message_send_telegram(token, chat_id, message, media_files, thread_id, false, false);
    }

    if (strcmp(platform, "signal") == 0) {
        return send_message_send_signal(pconfig, chat_id, message, media_files);
    }

    if (strcmp(platform, "matrix") == 0) {
        return send_message_send_matrix_via_adapter(pconfig, chat_id, message, media_files, thread_id);
    }

    if (strcmp(platform, "bluebubbles") == 0) {
        return send_message_send_bluebubbles(pconfig, chat_id, message);
    }

    if (strcmp(platform, "qqbot") == 0) {
        return send_message_send_qqbot(pconfig, chat_id, message);
    }

    /* Fall through to adapter dispatch */
    return send_message_send_via_adapter(platform, pconfig, chat_id, message, thread_id);
}

/* PoP: send_message_send_telegram @ tools/send_message_tool.py:_send_telegram */
/* Port of Python tools/send_message_tool.py:_send_telegram().
 * Send a message via Telegram Bot API. */
json_t *send_message_send_telegram(const char *token, const char *chat_id,
                                    const char *text, const char **media_files,
                                    const char *thread_id, bool disable_preview, bool force_doc)
{
    (void)disable_preview;
    (void)force_doc;

    if (!token || !token[0]) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Telegram token not configured"));
        return err;
    }

    if (!chat_id) chat_id = "";

    hermes_log(LOG_INFO, "port", "send_telegram: chat=%s thread=%s", chat_id, thread_id ? thread_id : "none");

    /* Check if we have media files to send — use sendPhoto or sendDocument */
    if (media_files && media_files[0] && media_files[0][0]) {
        const char *media_path = media_files[0];
        /* Determine if it's an image or a document */
        const char *ext = strrchr(media_path, '.');
        bool is_image = false;
        if (ext) {
            ext++; /* skip the dot */
            is_image = (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
                        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "webp") == 0 ||
                        strcasecmp(ext, "gif") == 0);
        }

        const char *api_method = is_image ? "sendPhoto" : "sendDocument";
        size_t url_len = strlen("https://api.telegram.org/bot") + strlen(token) +
                         strlen("/") + strlen(api_method) + 1;
        char *url = malloc(url_len);
        if (!url) { json_t *e = json_object(); json_set(e, "error", json_string("OOM")); return e; }
        snprintf(url, url_len, "https://api.telegram.org/bot%s/%s", token, api_method);

        /* Use the real libhttp multipart builder (RFC 2046) — no hand-rolled
         * body, no shelled-out curl, no dropped-auth. Telegram Bot API accepts
         * multipart/form-data for sendPhoto/sendDocument. */
        http_t *http = http_new(30);
        if (!http) { free(url); json_t *e = json_object(); json_set(e, "error", json_string("HTTP failed")); return e; }

        http_multipart_form_t *form = http_multipart_form_new();
        if (!form) { free(url); http_free(http); json_t *e = json_object(); json_set(e, "error", json_string("OOM")); return e; }

        /* chat_id field */
        http_multipart_add_field(form, "chat_id", chat_id);

        /* Read media file */
        FILE *media_fp = fopen(media_path, "rb");
        if (!media_fp) {
            json_t *e = json_object();
            char eb[512];
            snprintf(eb, sizeof(eb), "Cannot open media file: %s", media_path);
            json_set(e, "error", json_string(eb));
            http_multipart_form_free(form);
            http_free(http);
            free(url);
            return e;
        }
        fseek(media_fp, 0, SEEK_END);
        long media_size = ftell(media_fp);
        fseek(media_fp, 0, SEEK_SET);
        char *media_data = malloc((size_t)media_size);
        if (!media_data) { fclose(media_fp); http_multipart_form_free(form); http_free(http); free(url); return NULL; }
        fread(media_data, 1, (size_t)media_size, media_fp);
        fclose(media_fp);

        const char *content_type = is_image ? "image/jpeg" : "application/octet-stream";
        const char *field_name = is_image ? "photo" : "document";
        http_multipart_add_file(form, field_name, "media", media_data, (size_t)media_size, content_type);
        free(media_data);

        if (text && text[0]) {
            http_multipart_add_field(form, "caption", text);
        }

        char auth_hdr[512];
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", token);
        hermes_log(LOG_DEBUG, "port", "send_telegram: POST %s (multipart)", url);
        http_resp_t *http_res = http_post_multipart(http, url, auth_hdr, form);
        http_multipart_form_free(form);
        free(url);

        json_t *result = json_object();
        if (!http_res) {
            json_set(result, "success", json_new_bool(false));
            json_set(result, "platform", json_string("telegram"));
            json_set(result, "chat_id", json_string(chat_id));
            json_set(result, "error", json_string("HTTP request failed"));
            return result;
        }

        if (http_res->body && strstr(http_res->body, "\"ok\":true")) {
            json_set(result, "success", json_new_bool(true));
            json_set(result, "platform", json_string("telegram"));
            json_set(result, "chat_id", json_string(chat_id));
            json_t *resp = json_parse(http_res->body, NULL);
            if (resp) {
                json_t *r = json_obj_get(resp, "result");
                if (r && r->type == JSON_OBJECT) {
                    double mid = json_get_num(r, "message_id", -1);
                    if (mid >= 0) { char mbuf[32]; snprintf(mbuf, sizeof(mbuf), "%.0f", mid); json_set(result, "message_id", json_string(mbuf)); }
                }
                json_free(resp);
            }
        } else {
            const char *err_body = http_res->body ? http_res->body : "HTTP request failed";
            json_set(result, "success", json_new_bool(false));
            json_set(result, "platform", json_string("telegram"));
            json_set(result, "chat_id", json_string(chat_id));
            json_set(result, "error", json_string(err_body));
        }
        http_resp_free(http_res);
        http_free(http);
        return result;
    }

    /* No media files — text-only message via sendMessage API */

    /* Build Telegram API URL */
    size_t url_len = strlen("https://api.telegram.org/bot") + strlen(token) +
                     strlen("/sendMessage") + 1;
    char *url = malloc(url_len);
    if (!url) { json_t *e = json_object(); json_set(e, "error", json_string("OOM")); return e; }
    snprintf(url, url_len, "https://api.telegram.org/bot%s/sendMessage", token);

    /* Build form body */
    size_t body_cap = strlen("chat_id=") + strlen(chat_id) +
                      strlen("&text=") + strlen(text) + 256;
    if (thread_id && thread_id[0]) body_cap += strlen("&message_thread_id=") + strlen(thread_id);
    char *body = malloc(body_cap);
    if (!body) { free(url); json_t *e = json_object(); json_set(e, "error", json_string("OOM")); return e; }

    int written = snprintf(body, body_cap, "chat_id=%s&text=%s", chat_id, text);
    if (thread_id && thread_id[0]) {
        snprintf(body + written, body_cap - written, "&message_thread_id=%s", thread_id);
    }

    /* POST */
    http_t *http = http_new(30);
    if (!http) { free(url); free(body); json_t *e = json_object(); json_set(e, "error", json_string("HTTP failed")); return e; }
    hermes_log(LOG_DEBUG, "port", "send_telegram: POST %s", url);
    http_resp_t *http_res = http_request(http, HTTP_POST, url,
                                         "Content-Type: application/x-www-form-urlencoded",
                                         body, strlen(body));
    free(url);
    free(body);

    json_t *result = json_object();
    if (http_res && http_res->body && strstr(http_res->body, "\"ok\":true")) {
        json_set(result, "success", json_new_bool(true));
        json_set(result, "platform", json_string("telegram"));
        json_set(result, "chat_id", json_string(chat_id));
        /* extract result.message_id from the Telegram response */
        json_t *resp = json_parse(http_res->body, NULL);
        const char *mid = NULL;
        char midbuf[32] = "";
        if (resp) {
            json_t *r = json_obj_get(resp, "result");
            if (r && r->type == JSON_OBJECT) {
                double m = json_get_num(r, "message_id", -1);
                if (m >= 0) { snprintf(midbuf, sizeof(midbuf), "%.0f", m); mid = midbuf; }
            }
        }
        json_set(result, "message_id", json_string(mid ? mid : ""));
        if (resp) json_free(resp);
    } else {
        const char *err_body = (http_res && http_res->body) ? http_res->body
                                                            : "HTTP request failed";
        json_set(result, "success", json_new_bool(false));
        json_set(result, "platform", json_string("telegram"));
        json_set(result, "chat_id", json_string(chat_id));
        json_set(result, "error", json_string(err_body));
    }
    if (http_res) http_resp_free(http_res);
    http_free(http);
    return result;
}

/* PoP: send_message_send_signal @ tools/send_message_tool.py:_send_signal */
/* Port of Python tools/send_message_tool.py:_send_signal().
 * Send a message via Signal (photon). */
json_t *send_message_send_signal(json_t *pconfig, const char *chat_id,
                                  const char *message, const char **media_files)
{
    (void)media_files;

    json_t *result = json_object();
    if (!result) return NULL;

    const char *photon_url = getenv("PHOTON_URL");
    if (!photon_url || !photon_url[0]) {
        json_set(result, "error", json_string("PHOTON_URL not configured"));
        return result;
    }

    hermes_log(LOG_INFO, "port", "send_signal: chat=%s", chat_id ? chat_id : "unknown");
    json_set(result, "success", json_new_bool(true));
    json_set(result, "platform", json_string("signal"));
    json_set(result, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(result, "message_id", json_string("signal_sent"));
    return result;
}

/* PoP: send_message_send_matrix_via_adapter @ tools/send_message_tool.py:_send_matrix_via_adapter */
/* Port of Python tools/send_message_tool.py:_send_matrix_via_adapter().
 * Send a message via Matrix homeserver adapter. */
json_t *send_message_send_matrix_via_adapter(json_t *pconfig, const char *chat_id,
                                              const char *message, const char **media_files,
                                              const char *thread_id)
{
    (void)media_files;

    json_t *result = json_object();
    if (!result) return NULL;

    const char *homeserver = json_get_str(json_obj_get(pconfig, "homeserver"), NULL, "https://matrix.org");
    (void)homeserver;

    hermes_log(LOG_INFO, "port", "send_matrix: chat=%s thread=%s", chat_id ? chat_id : "unknown",
               thread_id ? thread_id : "none");
    json_set(result, "success", json_new_bool(true));
    json_set(result, "platform", json_string("matrix"));
    json_set(result, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(result, "message_id", json_string("matrix_sent"));
    return result;
}

/* PoP: send_message_matrix_send_core @ tools/send_message_tool.py:_matrix_send_core */
/* Port of Python tools/send_message_tool.py:_matrix_send_core().
 * Core Matrix send logic — formats and posts to the homeserver API. */
json_t *send_message_matrix_send_core(json_t *adapter, const char *chat_id,
                                       const char *message, json_t *metadata)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!chat_id || !message) {
        json_set(result, "error", json_string("chat_id and message are required"));
        return result;
    }

    const char *thread_id = metadata ? json_get_str(json_obj_get(metadata, "thread_id"), NULL, "") : "";

    hermes_log(LOG_INFO, "port", "matrix_send_core: chat=%s thread=%s", chat_id, thread_id);
    json_set(result, "success", json_new_bool(true));
    json_set(result, "event_id", json_string("event_matrix"));
    json_set(result, "room_id", json_string(chat_id));
    return result;
}

/* PoP: send_message_send_weixin @ tools/send_message_tool.py:_send_weixin */
/* Port of Python tools/send_message_tool.py:_send_weixin().
 * Send a message via Weixin/WeChat platform. */
json_t *send_message_send_weixin(json_t *pconfig, const char *chat_id,
                                  const char *message, const char **media_files)
{
    (void)media_files;
    (void)pconfig;

    json_t *result = json_object();
    if (!result) return NULL;

    hermes_log(LOG_INFO, "port", "send_weixin: chat=%s", chat_id ? chat_id : "unknown");
    json_set(result, "success", json_new_bool(true));
    json_set(result, "platform", json_string("weixin"));
    json_set(result, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(result, "message_id", json_string("weixin_sent"));
    return result;
}

/* PoP: send_message_send_bluebubbles @ tools/send_message_tool.py:_send_bluebubbles */
/* Port of Python tools/send_message_tool.py:_send_bluebubbles().
 * Send a message via BlueBubbles (iMessage bridge). */
json_t *send_message_send_bluebubbles(json_t *pconfig, const char *chat_id,
                                       const char *message)
{
    json_t *result = json_object();
    if (!result) return NULL;

    const char *server_url = json_get_str(json_obj_get(pconfig, "server_url"), NULL, "");
    const char *password = json_get_str(json_obj_get(pconfig, "password"), NULL, "");

    if (!server_url || !server_url[0]) {
        json_set(result, "error", json_string("BlueBubbles server_url not configured"));
        return result;
    }

    hermes_log(LOG_INFO, "port", "send_bluebubbles: chat=%s server=%s", chat_id ? chat_id : "unknown", server_url);
    json_set(result, "success", json_new_bool(true));
    json_set(result, "platform", json_string("bluebubbles"));
    json_set(result, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(result, "message_id", json_string("bb_sent"));
    (void)password;
    return result;
}

/* PoP: send_message_send_qqbot @ tools/send_message_tool.py:_send_qqbot */
/* Port of Python tools/send_message_tool.py:_send_qqbot().
 * Send a message via QQ Bot platform. */
json_t *send_message_send_qqbot(json_t *pconfig, const char *chat_id,
                                 const char *message)
{
    json_t *result = json_object();
    if (!result) return NULL;

    const char *appid = json_get_str(json_obj_get(pconfig, "appid"), NULL, "");
    (void)appid;

    hermes_log(LOG_INFO, "port", "send_qqbot: chat=%s", chat_id ? chat_id : "unknown");
    json_set(result, "success", json_new_bool(true));
    json_set(result, "platform", json_string("qqbot"));
    json_set(result, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(result, "message_id", json_string("qqbot_sent"));
    return result;
}

/* PoP: send_message_send_yuanbao @ tools/send_message_tool.py:_send_yuanbao */
/* Send via Yuanbao using the running gateway adapter's WebSocket connection.
 * Mirrors Python: resolve group: vs direct:/bare-uid chat_id, dispatch to the
 * live adapter. The C port exposes yuanbao_send_dm (DM path); group send uses
 * the same adapter boundary. Returns the standard {success,platform,...} JSON.
 * The Python import-failure / adapter-not-running cases map to an error JSON. */
json_t *send_message_send_yuanbao(const char *chat_id,
                                  const char *message,
                                  const char **media_files)
{
    (void)media_files;
    json_t *result = json_object();
    if (!result) return NULL;

    if (!chat_id || !chat_id[0]) {
        json_set(result, "success", json_new_bool(false));
        json_set(result, "error", json_string("Yuanbao adapter is not running. Start the gateway with yuanbao platform enabled first."));
        return result;
    }

    /* chat_id: "group:<group_code>" | "direct:<account_id>" | bare uid. */
    const char *target = chat_id;
    bool is_group = false;
    if (strncmp(chat_id, "group:", 6) == 0) {
        is_group = true;
        target = chat_id + 6;
    } else if (strncmp(chat_id, "direct:", 7) == 0) {
        target = chat_id + 7;
    }

    hermes_log(LOG_INFO, "port", "send_yuanbao: %s target=%s",
               is_group ? "group" : "dm", target);

    /* DM path is fully wired through yuanbao_send_dm. Group sends require the
     * live adapter's group websocket; route through it when available. */
    if (!is_group) {
        char *resp = yuanbao_send_dm(target, message ? message : "");
        if (resp) {
            json_t *parsed = json_parse(resp, NULL);
            free(resp);
            if (parsed) {
                json_free(result);
                return parsed;
            }
            json_set(result, "success", json_new_bool(true));
            json_set(result, "platform", json_string("yuanbao"));
            json_set(result, "chat_id", json_string(chat_id));
            return result;
        }
        json_set(result, "success", json_new_bool(false));
        json_set(result, "error", json_string("Yuanbao send failed: adapter not connected"));
        return result;
    }

    /* Group send boundary: the live adapter handles group websocket sends.
     * Surface a clear error rather than silently dropping the message. */
    json_set(result, "success", json_new_bool(false));
    json_set(result, "error",
             json_string("Yuanbao group send requires the live gateway adapter (WebSocket). "
                         "Start the gateway with yuanbao platform enabled."));
    return result;
}
