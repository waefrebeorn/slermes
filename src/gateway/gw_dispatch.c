/*
 * gw_dispatch.c -- extracted from gateway/server.c monolith.
 * Real implementation of one gateway-lifecycle concern. Public
 * gw_* protos stay in include/hermes_gateway.h; promoted cross-
 * module statics are in include/gw_server_internals.h.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_gateway_core.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "gateway_helpers.h"
#include "hermes_skill_commands.h"
#include "hermes_logger.h"
#include "hermes_telegram_filter.h"
#include "gw_server_internals.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

void gw_set_group_observe(const char *prefix, bool enabled) {
    if (prefix)
        snprintf(g_gw.group_observe_prefix, sizeof(g_gw.group_observe_prefix), "%s", prefix);
    g_gw.group_observe_enabled = enabled;
}

void gw_observe_append(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text || !*text) return;
    pthread_mutex_lock(&g_gw.observe_mutex);
    size_t cur = strlen(g_gw.observe_buffer);
    size_t add = strlen(platform) + 1 + strlen(chat_id) + 2 + strlen(text) + 3;
    if (cur + add >= sizeof(g_gw.observe_buffer)) {
        /* Buffer full — trim from front */
        char *nl = strchr(g_gw.observe_buffer, '\n');
        if (nl) {
            size_t remain = strlen(nl + 1);
            memmove(g_gw.observe_buffer, nl + 1, remain + 1);
            cur = remain;
        } else {
            g_gw.observe_buffer[0] = '\0';
            cur = 0;
        }
    }
    char entry[2048];
    snprintf(entry, sizeof(entry), "[%s:%s] %s\n", platform, chat_id, text);
    strncat(g_gw.observe_buffer, entry,
            sizeof(g_gw.observe_buffer) - strlen(g_gw.observe_buffer) - 1);
    pthread_mutex_unlock(&g_gw.observe_mutex);
}

void gw_register_pre_send(gw_hook_t hook, void *userdata) {
    if (gw_hooks.pre_send_count >= GW_HOOKS_MAX) return;
    gw_hooks.pre_send[gw_hooks.pre_send_count] = hook;
    gw_hooks.pre_send_data[gw_hooks.pre_send_count] = userdata;
    gw_hooks.pre_send_count++;
}

void gw_register_post_receive(gw_hook_t hook, void *userdata) {
    if (gw_hooks.post_receive_count >= GW_HOOKS_MAX) return;
    gw_hooks.post_receive[gw_hooks.post_receive_count] = hook;
    gw_hooks.post_receive_data[gw_hooks.post_receive_count] = userdata;
    gw_hooks.post_receive_count++;
}

void gw_register_interceptor(gw_hook_t hook, void *userdata) {
    if (gw_hooks.interceptor_count >= GW_HOOKS_MAX) return;
    gw_hooks.interceptor[gw_hooks.interceptor_count] = hook;
    gw_hooks.interceptor_data[gw_hooks.interceptor_count] = userdata;
    gw_hooks.interceptor_count++;
}

void gw_event_register(gw_event_listener_t listener, void *userdata) {
    if (gw_event_bus.count >= GW_EVENT_LISTENERS_MAX) return;
    gw_event_bus.listeners[gw_event_bus.count] = listener;
    gw_event_bus.data[gw_event_bus.count] = userdata;
    gw_event_bus.count++;
}

void gw_event_emit(const char *event_type, json_node_t *data) {
    for (int i = 0; i < gw_event_bus.count; i++) {
        gw_event_bus.listeners[i](event_type, data, gw_event_bus.data[i]);
    }
}

bool gw_retry_with_backoff(bool (*api_call)(void *ctx), void *ctx,
                                   int max_retries, int base_delay_ms) {
    for (int attempt = 0; attempt <= max_retries; attempt++) {
        if (api_call(ctx)) return true;
        if (attempt < max_retries) {
            int delay = base_delay_ms * (1 << attempt); /* exponential */
            /* Add jitter ±20% */
            delay += (int)(((double)rand() / RAND_MAX) * 2.0 * 0.2 * delay - 0.2 * delay);
            usleep(delay * 1000);
        }
    }
    return false;
}

bool gw_refresh_token(int plat_idx) {
    if (plat_idx < 0 || plat_idx >= GW_MAX_PLATFORMS) return false;
    /* Re-initialize the platform's HTTP client */
    if (g_gw.platform_http[plat_idx]) {
        http_client_free(g_gw.platform_http[plat_idx]);
    }
    g_gw.platform_http[plat_idx] = http_client_new(30);
    /* Apply proxy if configured */
    if (g_gw.proxy_enabled[plat_idx] && g_gw.platform_proxy[plat_idx][0]) {
        http_client_set_proxy(g_gw.platform_http[plat_idx],
                              g_gw.platform_proxy[plat_idx]);
    }
    gw_reconnect_reset(plat_idx);
    return true;
}

void gateway_send_fallback(const char *platform, const char *target,
                                   const char *text) {
    if (!platform || !target || !text) return;
    /* Strip all formatting and truncate */
    char *plain = gw_strip_all_formatting(text);
    char *truncated = gw_truncate_message(plain ? plain : text, 4000);
    if (truncated) {
        gateway_send(platform, target, truncated);
        free(truncated);
    }
    free(plain);
}

bool gw_try_send_media(const char *platform, const char *target, const char *text) {
    if (!text || strncmp(text, "MEDIA:", 6) != 0) return false;

    const char *path = text + 6;
    if (!path[0]) return false;

    /* Determine file type from extension */
    const char *ext = strrchr(path, '.');
    if (!ext) return false;

    /* Check if file exists */
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return false;

    if (strcmp(platform, "telegram") == 0) {
        /* Image extensions */
        if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".webp") == 0) {
            return telegram_send_photo(g_gw.http, target, path, NULL, NULL);
        }
        /* Audio extensions */
        if (strcasecmp(ext, ".ogg") == 0 || strcasecmp(ext, ".opus") == 0) {
            return telegram_send_voice(g_gw.http, target, path, NULL, NULL);
        }
        /* Video extensions */
        if (strcasecmp(ext, ".mp4") == 0 || strcasecmp(ext, ".mov") == 0 ||
            strcasecmp(ext, ".avi") == 0 || strcasecmp(ext, ".mkv") == 0) {
            return telegram_send_video(g_gw.http, target, path, NULL, NULL);
        }
        /* GIF/animation */
        if (strcasecmp(ext, ".gif") == 0) {
            return telegram_send_animation(g_gw.http, target, path, NULL, NULL);
        }
        /* Default: send as document */
        return telegram_send_document(g_gw.http, target, path, NULL, NULL);
    }

    /* For platforms without media APIs, fall back to text with path info */
    return false;
}

void gateway_send(const char *platform, const char *target, const char *text) {
    if (!platform || !target || !text) return;

    /* Apply registered message interceptors */
    char *intercepted = gw_apply_interceptors(platform, target, text);
    const char *send_text = intercepted ? intercepted : text;

    /* P186: Try MEDIA: prefix for file/media sends */
    if (gw_try_send_media(platform, target, send_text)) {
        free(intercepted);
        return;
    }

    /* P103: Try registered platform interface first */
    if (gw_platform_send(platform, target, send_text)) {
        free(intercepted);
        return;
    }

    /* Legacy fallback for unregistered platforms */
    bool sent = false;
    if (strcmp(platform, "telegram") == 0) {
        size_t len = strlen(send_text);
        if (len > 4000) {
            char chunk[4001];
            memcpy(chunk, send_text, 4000);
            chunk[4000] = '\0';
            telegram_send_message(g_gw.http, target, chunk, "Markdown", NULL, false, false, NULL);
            if (len > 4000)
                telegram_send_message(g_gw.http, target, send_text + 4000, "Markdown", NULL, false, false, NULL);
        } else {
            telegram_send_message(g_gw.http, target, send_text, "Markdown", NULL, false, false, NULL);
        }
        sent = true;
    } else if (strcmp(platform, "discord") == 0) {
        discord_send_message(g_gw.http, send_text);
        sent = true;
    } else if (strcmp(platform, "mattermost") == 0) {
        mattermost_send_message(g_gw.http, send_text);
        sent = true;
    }

    /* Fallback: if primary platform send failed, use plain-text fallback */
    if (!sent && send_text && *send_text)
        gateway_send_fallback(platform, target, send_text);

    free(intercepted);
}

void gateway_send_typing(const char *platform, const char *target) {
    if (!platform) return;

    /* P103: Try registered platform interface first */
    gw_platform_send_typing(platform, target);

    /* Legacy fallback */
    if (strcmp(platform, "telegram") == 0)
        telegram_send_chat_action(g_gw.http, target, "typing");
    else if (strcmp(platform, "discord") == 0)
        discord_send_typing(g_gw.http);
}

void gw_platform_register(const gw_platform_t *plat) {
    if (!plat || !plat->name) return;
    if (g_gw.platform_def_count >= GW_MAX_PLATFORMS) return;
    g_gw.platform_defs[g_gw.platform_def_count++] = *plat;
}

int gw_platform_get_count(void) {
    return g_gw.platform_def_count;
}

bool gw_platform_send(const char *platform_name, const char *chat_id, const char *text) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (!p || !p->send) return false;
    /* Apply pre-send hooks (may modify text) */
    if (gw_hooks.pre_send_count > 0) {
        char *modified = gw_apply_pre_send_hooks(platform_name, text);
        if (modified) {
            bool ok = p->send(chat_id, modified);
            free(modified);
            return ok;
        }
    }
    return p->send(chat_id, text);
}

void gw_platform_send_typing(const char *platform_name, const char *chat_id) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (p && p->send_typing)
        p->send_typing(chat_id);
}

bool gw_platform_send_reaction(const char *platform_name, const char *chat_id,
                                const char *message_id, const char *emoji) {
    gw_platform_t *p = gw_platform_find(platform_name);
    if (p && p->send_reaction)
        return p->send_reaction(chat_id, message_id, emoji);
    return false;
}

bool telegram_vtable_send_reaction(const char *chat_id,
                                           const char *message_id,
                                           const char *emoji) {
    return telegram_set_message_reaction(g_gw.http, chat_id, message_id, emoji);
}

void poll_platform_shutdown(void) {
    printf("[gateway] Polling platform shutdown\n");
}

void gw_platform_shutdown_all(void) {
    for (int i = 0; i < g_gw.platform_def_count; i++) {
        if (g_gw.platform_defs[i].shutdown)
            g_gw.platform_defs[i].shutdown();
    }
}

int gateway_tool_event_cb(const char *event_type, const char *tool_name,
                                  const char *tool_args, void *user_data) {
    (void)tool_args;
    gw_status_ctx_t *ctx = (gw_status_ctx_t *)user_data;
    if (!ctx || !ctx->platform || !ctx->chat_id) return 0;

    if (strcmp(event_type, "tool.started") == 0) {
        /* Throttle: don't send more than one status per 2 seconds */
        double now = gw_mono_time();
        if (now - ctx->last_status_ts < 2.0) return 0;
        ctx->last_status_ts = now;

        char msg[512];
        snprintf(msg, sizeof(msg), "⚙️ Running *%s*... ", tool_name ? tool_name : "tool");
        /* P161: Filter/sanitize status messages before platform delivery.
           Mirrors Python _prepare_gateway_status_message(). */
        char *filtered = gateway_prepare_status_message(ctx->platform, msg);
        if (filtered) {
            gateway_send(ctx->platform, ctx->chat_id, filtered);
            free(filtered);
        }
    }
    return 0;
}

int gateway_stream_cb(const char *token, void *user_data) {
    gw_status_ctx_t *ctx = (gw_status_ctx_t *)user_data;
    if (!ctx || !ctx->platform || !ctx->chat_id || !token) return 0;

    /* Accumulate token into buffer (truncated) */
    size_t tlen = strlen(token);
    if (ctx->stream_len + (int)tlen < (int)sizeof(ctx->stream_buf) - 1) {
        memcpy(ctx->stream_buf + ctx->stream_len, token, tlen);
        ctx->stream_len += (int)tlen;
        ctx->stream_buf[ctx->stream_len] = '\0';
    }
    ctx->stream_len += (int)tlen; /* always count total */

    /* Throttle: send typing indicator every ~5 seconds during streaming */
    double now = gw_mono_time();
    if (now - ctx->last_stream_ts >= 5.0) {
        ctx->last_stream_ts = now;
        gw_platform_send_typing(ctx->platform, ctx->chat_id);
    }
    return 0;
}
