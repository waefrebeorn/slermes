/*
 * constants.c — real C port of upstream Hermes plugins/platforms/qqbot/constants.py constant tables.
 * PoP: cn_5f6deb5549_constants @ plugins/platforms/qqbot/constants.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_5f6deb5549.h"

json_t *cn_5f6deb5549_constants(void) {
    json_t *out = json_object();
    json_set(out, "QQBOT_VERSION", json_string("1.1.0"));
    json_set(out, "API_BASE", json_string("https://api.sgroup.qq.com"));
    json_set(out, "TOKEN_URL", json_string("https://bots.qq.com/app/getAppAccessToken"));
    json_set(out, "GATEWAY_URL_PATH", json_string("/gateway"));
    json_set(out, "ONBOARD_CREATE_PATH", json_string("/lite/create_bind_task"));
    json_set(out, "ONBOARD_POLL_PATH", json_string("/lite/poll_bind_result"));
    json_set(out, "QR_URL_TEMPLATE", json_string("https://q.qq.com/qqbot/openclaw/connect.html?task_id={task_id}&_wv=2&source=hermes"));
    json_set(out, "DEFAULT_API_TIMEOUT", json_number(30));
    json_set(out, "FILE_UPLOAD_TIMEOUT", json_number(120));
    json_set(out, "CONNECT_TIMEOUT_SECONDS", json_number(20));
    json_set(out, "RECONNECT_BACKOFF", ({ json_t *__a = json_array();
        json_append(__a, json_number(2));
        json_append(__a, json_number(5));
        json_append(__a, json_number(10));
        json_append(__a, json_number(30));
        json_append(__a, json_number(60));
      __a; }));
    json_set(out, "MAX_RECONNECT_ATTEMPTS", json_number(100));
    json_set(out, "RATE_LIMIT_DELAY", json_number(60));
    json_set(out, "QUICK_DISCONNECT_THRESHOLD", json_number(5));
    json_set(out, "MAX_QUICK_DISCONNECT_COUNT", json_number(3));
    json_set(out, "ONBOARD_POLL_INTERVAL", json_number(2));
    json_set(out, "ONBOARD_API_TIMEOUT", json_number(10));
    json_set(out, "MAX_MESSAGE_LENGTH", json_number(4000));
    json_set(out, "DEDUP_WINDOW_SECONDS", json_number(300));
    json_set(out, "DEDUP_MAX_SIZE", json_number(1000));
    json_set(out, "MSG_TYPE_TEXT", json_number(0));
    json_set(out, "MSG_TYPE_MARKDOWN", json_number(2));
    json_set(out, "MSG_TYPE_MEDIA", json_number(7));
    json_set(out, "MSG_TYPE_INPUT_NOTIFY", json_number(6));
    json_set(out, "MEDIA_TYPE_IMAGE", json_number(1));
    json_set(out, "MEDIA_TYPE_VIDEO", json_number(2));
    json_set(out, "MEDIA_TYPE_VOICE", json_number(3));
    json_set(out, "MEDIA_TYPE_FILE", json_number(4));
    return out;
}
