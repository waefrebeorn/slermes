/*
 * gw_setup.c -- extracted from gateway/server.c monolith.
 * Real implementation of one gateway-lifecycle concern. Public
 * gw_* protos stay in include/hermes_gateway.h; promoted cross-
 * module statics are in include/gw_server_internals.h.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_gateway.h"
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

bool setup_telegram(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("telegram");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("TELEGRAM_BOT_TOKEN");
    if (!token) token = getenv("HERMES_TELEGRAM_TOKEN");
    if (!token) { fprintf(stderr, "Warning: TELEGRAM_BOT_TOKEN not set (set gateway.platforms.telegram.token in config.yaml or TELEGRAM_BOT_TOKEN env)\\n"); return false; }
    telegram_set_token(token);
    return true;
}

bool setup_discord(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("discord");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("DISCORD_BOT_TOKEN");
    const char *channel = getenv("DISCORD_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: DISCORD_BOT_TOKEN or DISCORD_CHANNEL_ID not set\n");
        return false;
    }
    discord_set_token(token);
    discord_set_channel(channel);
    return true;
}

bool setup_slack(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("slack");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("SLACK_BOT_TOKEN");
    const char *channel = getenv("SLACK_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: SLACK_BOT_TOKEN or SLACK_CHANNEL_ID not set\n");
        return false;
    }
    slack_set_token(token);
    slack_set_channel(channel);
    return true;
}

bool setup_matrix(void) {
    const char *hs = getenv("MATRIX_HOMESERVER");
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("matrix");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("MATRIX_ACCESS_TOKEN");
    const char *room = getenv("MATRIX_ROOM_ID");
    if (!token) { fprintf(stderr, "Warning: MATRIX_ACCESS_TOKEN not set\n"); return false; }
    matrix_set_homeserver(hs && hs[0] ? hs : "https://matrix.org");
    matrix_set_token(token);
    if (room) matrix_set_room(room);
    return true;
}

bool setup_mattermost(void) {
    const char *url = getenv("MATTERMOST_URL");
    const char *token = getenv("MATTERMOST_TOKEN");
    const char *channel = getenv("MATTERMOST_CHANNEL_ID");
    if (!token || !channel) {
        fprintf(stderr, "Warning: MATTERMOST_TOKEN or MATTERMOST_CHANNEL_ID not set\n");
        return false;
    }
    mattermost_set_url(url && url[0] ? url : "http://localhost:8065");
    mattermost_set_token(token);
    mattermost_set_channel(channel);
    return true;
}

bool setup_webhook(void) {
    const char *secret = getenv("WEBHOOK_SECRET");
    if (secret && *secret) {
        webhook_set_verify_secret(secret);
        printf("[webhook] HMAC verification: enabled\n");
    } else {
        printf("[webhook] HMAC verification: disabled (no WEBHOOK_SECRET)\n");
    }
    return true;
}

bool setup_whatsapp(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("whatsapp");
    const char *token = NULL;
    if (pc && pc->token[0]) token = pc->token;
    if (!token) token = pc && pc->api_key[0] ? pc->api_key : NULL;
    if (!token) token = getenv("WHATSAPP_TOKEN");
    const char *phone = getenv("WHATSAPP_PHONE_NUMBER_ID");
    const char *verify = getenv("WHATSAPP_VERIFY_TOKEN");
    if (!token || !phone) {
        fprintf(stderr, "Warning: WHATSAPP_TOKEN or WHATSAPP_PHONE_NUMBER_ID not set\n");
        return false;
    }
    whatsapp_set_token(token);
    whatsapp_set_phone_id(phone);
    if (verify) whatsapp_set_verify_token(verify);
    return true;
}

bool setup_email(void) {
    const char *from = getenv("EMAIL_FROM");
    if (from) email_set_from(from);

    /* Validate: email needs IMAP (for incoming) or SMTP/sendmail (for outgoing) */
    const char *imap = getenv("EMAIL_IMAP_SERVER");
    const char *smtp = getenv("EMAIL_SMTP_SERVER");
    const char *cmd = getenv("EMAIL_SEND_CMD");
    if (!imap && !smtp && !cmd) {
        fprintf(stderr, "Warning: neither EMAIL_IMAP_SERVER nor EMAIL_SMTP_SERVER nor"
                        " EMAIL_SEND_CMD set. Email will not function.\n");
        return false;
    }
    return true;
}

bool setup_signal(void) {
    const char *number = getenv("SIGNAL_NUMBER");
    const char *cli_path = getenv("SIGNAL_CLI_PATH");
    if (!number) {
        fprintf(stderr, "Warning: SIGNAL_NUMBER not set\n");
        return false;
    }
    signal_set_number(number);
    if (cli_path) signal_set_cli_path(cli_path);
    return true;
}

bool setup_api_server(void) {
    const hermes_platform_cfg_t *pc = hermes_config_get_platform("api_server");
    const char *api_key = NULL;
    if (pc && pc->api_key[0]) api_key = pc->api_key;
    if (!api_key) api_key = getenv("API_SERVER_KEY");
    if (!api_key) {
        fprintf(stderr, "Warning: API_SERVER_KEY not set (set gateway.platforms.api_server.key in config.yaml or API_SERVER_KEY env)\n");
        return false;
    }
    /* The API server adapter is now registered via register_api_server_platform()
     * in api_server_adapter.c. The connect() call in init will start the server thread. */
    return true;
}

bool setup_ha(void) {
    const char *url = getenv("HA_URL");
    const char *token = getenv("HA_TOKEN");
    if (!url || !token) {
        fprintf(stderr, "Warning: HA_URL and HA_TOKEN must be set\n");
        return false;
    }
    ha_set_url(url);
    ha_set_token(token);
    const char *entity = getenv("HA_NOTIFY_ENTITY");
    if (entity) ha_set_notify_entity(entity);
    return true;
}

bool setup_sms(void) {
    const char *sid = getenv("TWILIO_ACCOUNT_SID");
    const char *token = getenv("TWILIO_AUTH_TOKEN");
    const char *from = getenv("TWILIO_FROM_NUMBER");
    if (!sid || !from) {
        fprintf(stderr, "Warning: TWILIO_ACCOUNT_SID and TWILIO_FROM_NUMBER must be set\n");
        return false;
    }
    sms_set_twilio(sid, token, from);

    /* P111: Optional status callback URL for delivery status */
    const char *cb = getenv("TWILIO_STATUS_CALLBACK");
    if (cb) {
        sms_set_status_callback(cb);
        printf("[gateway] SMS status callbacks configured\n");
    }

    /* P111: Optional webhook path (default /sms-webhook on the webhook server) */
    const char *wh = getenv("TWILIO_WEBHOOK_PATH");
    if (wh) {
        sms_set_webhook_url(wh);
    }
    return true;
}

bool setup_feishu(void) {
    const char *url = getenv("FEISHU_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: FEISHU_WEBHOOK_URL not set\n");
        return false;
    }
    feishu_set_webhook(url);
    return true;
}

bool setup_wecom(void) {
    const char *url = getenv("WECOM_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: WECOM_WEBHOOK_URL not set\n");
        return false;
    }
    wecom_set_webhook(url);
    return true;
}

bool setup_dingtalk(void) {
    const char *url = getenv("DINGTALK_WEBHOOK_URL");
    if (!url) {
        fprintf(stderr, "Warning: DINGTALK_WEBHOOK_URL not set\n");
        return false;
    }
    dingtalk_set_webhook(url);
    return true;
}

bool setup_qqbot(void) {
    const char *url = getenv("QQ_BOT_WEBHOOK_URL");
    const char *token = getenv("QQ_BOT_TOKEN");
    if (!url) {
        fprintf(stderr, "Warning: QQ_BOT_WEBHOOK_URL not set\n");
        return false;
    }
    qqbot_set_webhook(url);
    if (token) qqbot_set_token(token);
    return true;
}

bool setup_bluebubbles(void) {
    const char *url = getenv("BLUEBUBBLES_URL");
    const char *pwd = getenv("BLUEBUBBLES_PASSWORD");
    if (!url || !pwd) {
        fprintf(stderr, "Warning: BLUEBUBBLES_URL and BLUEBUBBLES_PASSWORD must be set\n");
        return false;
    }
    bluebubbles_set_url(url);
    bluebubbles_set_password(pwd);
    return true;
}

int get_webhook_port(void) {
    /* 1. Config value (from YAML) takes priority */
    if (g_gw.config.webhook_port > 0 && g_gw.config.webhook_port <= 65535)
        return g_gw.config.webhook_port;
    /* 2. Env vars */
    const char *port_str = getenv("SLERMES_WEBHOOK_PORT");
    if (!port_str) port_str = getenv("HERMES_WEBHOOK_PORT");
    if (!port_str) port_str = getenv("WEBHOOK_PORT");
    int port = port_str ? atoi(port_str) : 8080;
    if (port <= 0 || port > 65535) port = 8080;
    return port;
}

bool setup_msgraph_webhook(void) {
    const char *port_str = getenv("MSGRAPH_WEBHOOK_PORT");
    int port = port_str ? atoi(port_str) : 8646;
    if (port <= 0 || port > 65535) port = 8646;
    msgraph_webhook_init(NULL, NULL, port);
    return true;
}

bool setup_weixin(void) {
    const char *token = getenv("WEIXIN_TOKEN");
    const char *account_id = getenv("WEIXIN_ACCOUNT_ID");
    if (!token || !account_id) {
        fprintf(stderr, "Warning: WEIXIN_TOKEN and WEIXIN_ACCOUNT_ID must be set\n");
        return false;
    }
    weixin_init(token, account_id);
    return true;
}

bool setup_yuanbao(void) {
    const char *app_id = getenv("YUANBAO_APP_ID");
    const char *app_secret = getenv("YUANBAO_APP_SECRET");
    const char *bot_id = getenv("YUANBAO_BOT_ID");
    const char *ws_url = getenv("YUANBAO_WS_URL");
    const char *api_domain = getenv("YUANBAO_API_DOMAIN");
    if (!app_id || !app_secret) {
        fprintf(stderr, "Warning: YUANBAO_APP_ID and YUANBAO_APP_SECRET must be set\n");
        return false;
    }
    return yuanbao_init(app_id, app_secret, bot_id, ws_url, api_domain);
}

int cmd_gateway_status(void) {
    hermes_config_t cfg;
    if (!hermes_config_load(&cfg, NULL)) {
        printf("No config loaded\n");
        return 1;
    }

    printf("=== Gateway Status ===\n\n");

    printf("Configured platforms: ");
    if (cfg.gateway_platforms[0])
        printf("%s\n", cfg.gateway_platforms);
    else
        printf("(none in config)\n");

    printf("Env HERMES_GATEWAY_PLATFORMS: ");
    const char *env = getenv("HERMES_GATEWAY_PLATFORMS");
    if (env) printf("%s\n", env); else printf("(not set)\n");

    printf("Default platform: telegram\n");

    /* Check key env vars per platform type */
    static const char *platform_keys[][2] = {
        {"telegram", "TELEGRAM_BOT_TOKEN"},
        {"discord",  "DISCORD_BOT_TOKEN"},
        {"slack",    "SLACK_BOT_TOKEN"},
        {"signal",   "SIGNAL_NUMBER"},
        {"sms",      "TWILIO_ACCOUNT_SID"},
        {"matrix",   "MATRIX_HOMESERVER"},
        {NULL, NULL}
    };

    printf("\nCredentials check:\n");
    for (int i = 0; platform_keys[i][0]; i++) {
        const char *val = getenv(platform_keys[i][1]);
        printf("  %-12s %s %s\n", platform_keys[i][0],
               val ? "✅" : "❌", val ? "(found)" : "missing");
    }

    printf("\nGateway: ready to start with `slermes gateway start`\n");
    return 0;
}

int cmd_gateway_list(void) {
    static const char *platforms[] = {
        "telegram", "discord", "slack", "matrix", "mattermost",
        "webhook", "whatsapp", "email", "signal", "homeassistant",
        "sms", "api_server", "feishu", "wecom", "dingtalk",
        "qqbot", "bluebubbles", "msgraph_webhook", "weixin", "yuanbao",
        NULL
    };
    static const char *descriptions[] = {
        "Telegram bot API polling", "Discord gateway bot", "Slack RTM/Events API",
        "Matrix client-server API", "Mattermost webhooks",
        "Generic HTTP webhook receiver", "WhatsApp Cloud API webhook",
        "IMAP/SMTP email client", "Signal CLI over dbus",
        "Home Assistant long-lived token API",
        "Twilio SMS gateway", "REST API server",
        "Feishu/Lark bot API", "WeCom (WeChat Work) bot API",
        "DingTalk bot API",
        "QQ Bot API (OneBot/QQ Guild)", "BlueBubbles iMessage API",
        "Microsoft Graph API webhook", "Weixin Official Account",
        "Yuanbao (Tencent) protobuf protocol",
        NULL
    };

    printf("=== Available Gateway Platforms ===\n\n");
    for (int i = 0; platforms[i]; i++)
        printf("  %-20s %s\n", platforms[i], descriptions[i]);
    printf("\n%d platform types available\n", 20);
    printf("Usage: slermes gateway [start|--platform <name>]\n");
    return 0;
}
