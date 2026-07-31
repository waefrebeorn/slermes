/**
 * @defgroup hermes_gateway Gateway
 * @brief Multi-platform messaging gateway.
 *
 * Routes messages between 19+ platforms and the agent loop.
 * Handles platform adapters, message queues, rate limiting,
 * markdown formatting, and media attachments.
 *
 * @{
 */
#ifndef HERMES_GATEWAY_H
#define HERMES_GATEWAY_H

/* ================================================================
 *  Core types (self-contained, no platform APIs)
 * ================================================================ */
#include "hermes_gateway_types.h"

/* ================================================================
 *  Core gateway APIs
 * ================================================================ */
#include "hermes_gateway_core.h"
#include "hermes_gateway_queue.h"
#include "hermes_gateway_rate_limiter.h"
#include "hermes_gateway_pool.h"
#include "hermes_gateway_platform.h"
#include "hermes_gateway_forensics.h"
#include "hermes_gateway_sticker_cache.h"
#include "hermes_gateway_mirror.h"
#include "hermes_gateway_slash_access.h"
#include "hermes_gateway_delivery.h"
#include "hermes_gateway_display_config.h"
#include "hermes_gateway_runtime_footer.h"
#include "hermes_gateway_channel_directory.h"
#include "hermes_gateway_pairing.h"
#include "hermes_gateway_memory_monitor.h"

/* ================================================================
 *  Platform-specific APIs (include only what you need)
 * ================================================================ */

/* Telegram */
#include "hermes_gateway_telegram.h"

/* Discord */
#include "hermes_gateway_discord.h"

/* Webhook / API Server */
#include "hermes_gateway_webhook.h"

/* Slack */
#include "hermes_gateway_slack.h"

/* Matrix */
#include "hermes_gateway_matrix.h"

/* Mattermost */
#include "hermes_gateway_mattermost.h"

/* WhatsApp */
#include "hermes_gateway_whatsapp.h"

/* Email */
#include "hermes_gateway_email.h"

/* Signal */
#include "hermes_gateway_signal.h"

/* HomeAssistant */
#include "hermes_gateway_homeassistant.h"

/* SMS / Twilio */
#include "hermes_gateway_sms.h"

/* Feishu (Lark) */
#include "hermes_gateway_feishu.h"

/* WeCom (WeChat Work) */
#include "hermes_gateway_wecom.h"

/* DingTalk */
#include "hermes_gateway_dingtalk.h"

/* QQ Bot */
#include "hermes_gateway_qqbot.h"

/* BlueBubbles (iMessage) */
#include "hermes_gateway_bluebubbles.h"

/* Microsoft Graph Webhook */
#include "hermes_gateway_msgraph.h"

/* Weixin (iLink Bot API) */
#include "hermes_gateway_weixin.h"

/* Yuanbao */
#include "hermes_gateway_yuanbao.h"

/** @} */
#endif /* HERMES_GATEWAY_H */
/* Entry point — gateway main */
int hermes_gateway_main(int argc, char **argv);
