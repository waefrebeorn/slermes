/*
 * constants.c — C home for upstream Hermes plugins/platforms/qqbot/constants.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: plugins_platforms_qqbot_constants_pending @ plugins/platforms/qqbot/constants.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const plugins_platforms_qqbot_constants_CONST_API_BASE = "https://api.sgroup.qq.com";
static const char *const plugins_platforms_qqbot_constants_CONST_TOKEN_URL = "https://bots.qq.com/app/getAppAccessToken";
static const char *const plugins_platforms_qqbot_constants_CONST_GATEWAY_URL_PATH = "/gateway";
static const char *const plugins_platforms_qqbot_constants_CONST_ONBOARD_CREATE_PATH = "/lite/create_bind_task";
static const char *const plugins_platforms_qqbot_constants_CONST_ONBOARD_POLL_PATH = "/lite/poll_bind_result";
static const char *const plugins_platforms_qqbot_constants_CONST_QR_URL_TEMPLATE = "https://q.qq.com/qqbot/openclaw/connect.html?task_id={task_id}&_wv=2&source=hermes";
static long long plugins_platforms_qqbot_constants_CONST_MAX_RECONNECT_ATTEMPTS = 100;
static long long plugins_platforms_qqbot_constants_CONST_MAX_QUICK_DISCONNECT_COUNT = 3;
static long long plugins_platforms_qqbot_constants_CONST_MAX_MESSAGE_LENGTH = 4000;
static long long plugins_platforms_qqbot_constants_CONST_DEDUP_WINDOW_SECONDS = 300;
static long long plugins_platforms_qqbot_constants_CONST_DEDUP_MAX_SIZE = 1000;
static long long plugins_platforms_qqbot_constants_CONST_MSG_TYPE_TEXT = 0;
static long long plugins_platforms_qqbot_constants_CONST_MSG_TYPE_MARKDOWN = 2;
static long long plugins_platforms_qqbot_constants_CONST_MSG_TYPE_MEDIA = 7;
static long long plugins_platforms_qqbot_constants_CONST_MSG_TYPE_INPUT_NOTIFY = 6;
static long long plugins_platforms_qqbot_constants_CONST_MEDIA_TYPE_IMAGE = 1;
static long long plugins_platforms_qqbot_constants_CONST_MEDIA_TYPE_VIDEO = 2;
static long long plugins_platforms_qqbot_constants_CONST_MEDIA_TYPE_VOICE = 3;
static long long plugins_platforms_qqbot_constants_CONST_MEDIA_TYPE_FILE = 4;

void plugins_platforms_qqbot_constants_port_pending(void) { (void)0; }
