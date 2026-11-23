/*
 * adapter.c — C home for upstream Hermes plugins/platforms/line/adapter.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: plugins_platforms_line_adapter_pending @ plugins/platforms/line/adapter.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const plugins_platforms_line_adapter_CONST_LINE_REPLY_URL = "https://api.line.me/v2/bot/message/reply";
static const char *const plugins_platforms_line_adapter_CONST_LINE_PUSH_URL = "https://api.line.me/v2/bot/message/push";
static const char *const plugins_platforms_line_adapter_CONST_LINE_LOADING_URL = "https://api.line.me/v2/bot/chat/loading/start";
static const char *const plugins_platforms_line_adapter_CONST_LINE_CONTENT_URL_FMT = "https://api-data.line.me/v2/bot/message/{message_id}/content";
static const char *const plugins_platforms_line_adapter_CONST_LINE_BOT_INFO_URL = "https://api.line.me/v2/bot/info";
static const char *const plugins_platforms_line_adapter_CONST_LINE_MAX_MESSAGES_PER_CALL = 5;
static const char *const plugins_platforms_line_adapter_CONST_DEFAULT_WEBHOOK_PORT = 8646;
static const char *const plugins_platforms_line_adapter_CONST_DEFAULT_WEBHOOK_PATH = "/line/webhook";
static const char *const plugins_platforms_line_adapter_CONST_DEFAULT_MEDIA_PATH_PREFIX = "/line/media";
static const char *const plugins_platforms_line_adapter_CONST_DEFAULT_PENDING_REPLY_TEXT_KEY = "platform.line.pending.still_thinking";
static const char *const plugins_platforms_line_adapter_CONST_DEFAULT_DELIVERED_TEXT_KEY = "platform.line.pending.delivered";
static const char *const plugins_platforms_line_adapter_CONST_DEFAULT_INTERRUPTED_TEXT_KEY = "platform.line.pending.interrupted";
static const char *const plugins_platforms_line_adapter_CONST_DEFAULT_EXPIRED_TEXT_KEY = "platform.line.pending.expired";

void plugins_platforms_line_adapter_port_pending(void) { (void)0; }
