/*
 * gw_pollers.c — per-platform gateway polling / bridge thread functions.
 * Extracted from gateway/server.c (monolith split): each platform's
 * long-poll loop (Telegram, Discord, Slack, Matrix, Mattermost, Email,
 * Signal, Home Assistant, SMS, Feishu, WeCom, DingTalk, QQ Bot,
 * BlueBubbles), the webhook/msgraph server threads, and the
 * weixin/yuanbao bridge threads. Referenced only via the platform
 * dispatch table in hermes_gateway_main().
 */

#include "gw_pollers.h"
#include "gw_server_internals.h"
#include "hermes_telegram_filter.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Telegram-specific: poll for a response from a specific chat_id.
   Called during approval wait to short-poll Telegram for user's y/n/a
   response. Returns strdup'd text or NULL. */
char *telegram_poll_for_response(const char *target_chat_id) {
    if (!target_chat_id) return NULL;

    json_node_t *root = telegram_get_updates(g_gw.http, g_gw.tg_offset, 5);
    if (!root) return NULL;

    json_node_t *result = json_obj_get(root, "result");
    char *response = NULL;
    if (result && json_len(result) > 0) {
        size_t n = json_len(result);
        for (size_t i = 0; i < n; i++) {
            json_node_t *update = json_get(result, i);
            double update_id = json_get_num(update, "update_id", 0);
            if (update_id > 0)
                g_gw.tg_offset = (int)update_id + 1;

            const char *chat_id = telegram_get_chat_id(update);
            const char *text = telegram_get_text(update);
            if (chat_id && text && strcmp(chat_id, target_chat_id) == 0) {
                response = strdup(text);
                break;
            }
        }
    }
    json_free(root);
    return response;
}

void *thread_poll_telegram(void *arg) {
    (void)arg;
    printf("[gateway] Telegram polling (interval: %ds)\n", g_gw.poll_interval);

    /* L08: Fetch bot identity on first run for @mention detection */
    telegram_get_me(g_gw.http);
    if (telegram_get_username()[0])
        printf("[gateway] Telegram bot: @%s\n", telegram_get_username());

    /* Register approval poll function for Telegram.
       During approval wait, the gateway will short-poll Telegram for the response. */
    gw_approval_set_poll(telegram_poll_for_response, 1);

    while (g_gw.running) {
        json_node_t *root = telegram_get_updates(g_gw.http, g_gw.tg_offset, 30);

        if (root) {
            json_node_t *result = json_obj_get(root, "result");
            if (result && json_len(result) > 0) {
                size_t n = json_len(result);
                for (size_t i = 0; i < n; i++) {
                    json_node_t *update = json_get(result, i);
                    double update_id = json_get_num(update, "update_id", 0);
                    if (update_id > 0)
                        g_gw.tg_offset = (int)update_id + 1;

                    const char *chat_id = telegram_get_chat_id(update);
                    const char *text = telegram_get_text(update);
                    const char *thread_id = telegram_get_message_thread_id(update);
                    const char *message_id = NULL;
                    if (!chat_id || !text) continue;

                    /* ── Extract message_id from the update for source metadata ── */
                    json_node_t *msg = json_obj_get(update, "message");
                    if (!msg) msg = json_obj_get(update, "edited_message");
                    if (msg) {
                        const char *mid = json_get_str(msg, "message_id", NULL);
                        if (mid) message_id = mid;
                    }

                    /* ── Telegram message filtering (port of Python TelegramAdapter) ── */
                    bool is_group = telegram_is_group(update);
                    bool is_mentioned = telegram_is_mentioned(update);
                    const char *bot_username = telegram_get_username();
                    bool is_reply = false; /* TODO: detect reply_to_bot */

                    /* Determine observe vs process */
                    bool should_observe = tg_should_observe_message(
                        telegram_get_chat_type(update), chat_id, text, thread_id,
                        is_group, is_mentioned, is_reply, bot_username);

                    bool should_process = tg_should_process_message(
                        telegram_get_chat_type(update), chat_id, text, thread_id,
                        is_group, is_mentioned, is_reply, bot_username);

                    if (!should_process && !should_observe) {
                        /* Silently skip (not in allowed_chats, guest mode off, etc.) */
                        if (g_gw.group_observe_enabled)
                            printf("[gateway] Skipped (filtered): %s %s\n", chat_id, text);
                        continue;
                    }

                    /* L08: Group observe — silently accumulate unmentioned group messages */
                    if (should_observe) {
                        /* Build attributed text: [username|user_id]\ntext */
                        const char *user_id = telegram_get_user_id(update);
                        const char *user_name = telegram_get_user_name(update);
                        char observe_buf[8192];
                        if (user_name && user_id) {
                            snprintf(observe_buf, sizeof(observe_buf),
                                     "[%s|%s]\n%s", user_name, user_id, text);
                        } else {
                            snprintf(observe_buf, sizeof(observe_buf), "%s", text);
                        }
                        gw_observe_append("telegram", chat_id, observe_buf);
                        printf("[gateway] Observed (no trigger): %s: %s\n", chat_id, observe_buf);
                        continue;
                    }

                    process_update("telegram", chat_id, text);

                    /* P102a: Populate session source metadata from Telegram update */
                    gw_session_source_t src;
                    session_source_set(&src, "telegram", chat_id,
                                       telegram_get_chat_name(update),
                                       telegram_get_chat_type(update),
                                       telegram_get_user_id(update),
                                       telegram_get_user_name(update),
                                       thread_id, /* thread_id for topics */
                                       NULL, /* chat_topic */
                                       NULL, /* user_id_alt */
                                       NULL, /* chat_id_alt */
                                       NULL, /* guild_id */
                                       NULL, /* parent_chat_id */
                                       message_id, /* message_id */
                                       telegram_is_bot(update));
                    gw_session_set_source("telegram", chat_id, &src);
                }
            }
            json_free(root);
            /* Successful poll — reset reconnect backoff */
            gw_reconnect_reset(0);
        } else {
            /* Poll failed — exponential backoff reconnect */
            double delay = gw_reconnect_delay(0);
            printf("[gateway] Poll failed, reconnecting in %.0fs\n", delay);
            if (g_gw.running) sleep((int)delay);
        }

        /* Drain queued messages (e.g., from rate-limiting on prior cycles) */
        if (gw_queue_depth() > 0)
            gw_queue_drain_all();

        if (g_gw.running)
            sleep(g_gw.poll_interval);
    }
    return NULL;
}

void *thread_poll_discord(void *arg) {
    (void)arg;
    printf("[gateway] Discord polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = discord_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("discord",
                               discord_get_chat_id(update),
                               discord_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

void *thread_poll_slack(void *arg) {
    (void)arg;
    printf("[gateway] Slack polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = slack_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("slack",
                               slack_get_chat_id(update),
                               slack_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

void *thread_poll_matrix(void *arg) {
    (void)arg;
    printf("[gateway] Matrix polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = matrix_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("matrix",
                               matrix_get_chat_id(update),
                               matrix_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

void *thread_poll_mattermost(void *arg) {
    (void)arg;
    printf("[gateway] Mattermost polling (interval: %ds)\n", g_gw.poll_interval);

    while (g_gw.running) {
        json_node_t *updates = mattermost_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("mattermost",
                               mattermost_get_chat_id(update),
                               mattermost_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

void *thread_webhook(void *arg) {
    int port = *(int *)arg;
    printf("[gateway] Webhook HTTP API on port %d\n", port);
    webhook_server_run(port);
    return NULL;
}

/* Email poll thread */
void *thread_poll_email(void *arg) {
    (void)arg;
    int poll_int = g_gw.poll_interval * 3; /* Email polls less frequently */
    printf("[gateway] Email polling (interval: %ds)\n", poll_int);
    while (g_gw.running) {
        json_node_t *updates = email_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("email",
                               email_get_chat_id(update),
                               email_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(poll_int);
    }
    return NULL;
}

/* Signal poll thread */
void *thread_poll_signal(void *arg) {
    (void)arg;
    /* Check if signal-cli is available */
    if (!signal_check_available()) {
        printf("[gateway] signal-cli not found. Signal platform disabled.\n");
        return NULL;
    }
    printf("[gateway] Signal polling (interval: %ds)\n", g_gw.poll_interval);
    while (g_gw.running) {
        json_node_t *updates = signal_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("signal",
                               signal_get_chat_id(update),
                               signal_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

void *thread_poll_ha(void *arg) {
    (void)arg;
    printf("[gateway] HomeAssistant polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = ha_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("homeassistant",
                               ha_get_chat_id(update),
                               ha_get_text(update));
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

void *thread_poll_sms(void *arg) {
    (void)arg;
    printf("[gateway] SMS/Twilio polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = sms_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("sms",
                               sms_get_chat_id(update),
                               sms_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

void *thread_poll_feishu(void *arg) {
    (void)arg;
    printf("[gateway] Feishu platform (webhook-based). Idle.\n");
    while (g_gw.running) sleep(g_gw.poll_interval * 10);
    return NULL;
}

void *thread_poll_wecom(void *arg) {
    (void)arg;
    printf("[gateway] WeCom polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = wecom_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("wecom",
                               wecom_get_chat_id(update),
                               wecom_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

void *thread_poll_dingtalk(void *arg) {
    (void)arg;
    printf("[gateway] DingTalk polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = dingtalk_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("dingtalk",
                               dingtalk_get_chat_id(update),
                               dingtalk_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

void *thread_poll_qqbot(void *arg) {
    (void)arg;
    printf("[gateway] QQ Bot polling (interval: %ds)\n", g_gw.poll_interval * 5);
    while (g_gw.running) {
        json_node_t *updates = qqbot_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                process_update("qqbot",
                               qqbot_get_chat_id(update),
                               qqbot_get_text(update));
            }
            json_free(updates);
        } else {
            json_free(updates);
        }
        if (g_gw.running) sleep(g_gw.poll_interval * 5);
    }
    return NULL;
}

void *thread_poll_bluebubbles(void *arg) {
    (void)arg;
    printf("[gateway] BlueBubbles platform (iMessage). Polling every %ds.\\n", g_gw.poll_interval);

    /* Check for optional poll GUID env var */
    const char *poll_guid = getenv("BLUEBUBBLES_POLL_GUID");
    if (poll_guid) {
        bluebubbles_set_poll_guid(poll_guid);
        printf("[gateway] BlueBubbles polling chat GUID: %s\\n", poll_guid);
    } else {
        bluebubbles_set_poll_guid(NULL);
        printf("[gateway] BlueBubbles is webhook-driven. Set BLUEBUBBLES_POLL_GUID for polling.\\n");
    }

    while (g_gw.running) {
        json_node_t *updates = bluebubbles_poll_messages(g_gw.http);
        if (updates && json_len(updates) > 0) {
            size_t n = json_len(updates);
            for (size_t i = 0; i < n; i++) {
                json_node_t *update = json_get(updates, i);
                const char *chat_id = bluebubbles_get_chat_id(update);
                const char *text = bluebubbles_get_text(update);
                if (chat_id && text && text[0]) {
                    process_update("bluebubbles", chat_id, text);
                }
            }
        }
        json_free(updates);
        if (g_gw.running) sleep(g_gw.poll_interval);
    }
    return NULL;
}

void *thread_msgraph_webhook(void *arg) {
    (void)arg;
    msgraph_webhook_run();
    return NULL;
}

void *thread_weixin(void *arg) {
    (void)arg;
    weixin_start();
    return NULL;
}

void *thread_yuanbao(void *arg) {
    (void)arg;
    yuanbao_start();
    return NULL;
}
