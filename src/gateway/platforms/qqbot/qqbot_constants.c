/*
 * qqbot_constants.c — canonical C11 port of
 * gateway/platforms/qqbot/constants.py.
 *
 * QQBot package-level constants shared across adapter, onboard, and other
 * modules. Env-overridable values (PORTAL_HOST) read at first call and cache.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: qqbot_constants_version @ gateway/platforms/qqbot/constants.py:QQBOT_VERSION */
const char *qqbot_constants_version(void) { return "1.1.0"; }

/* PoP: qqbot_constants_portal_host @ gateway/platforms/qqbot/constants.py:PORTAL_HOST */
const char *qqbot_constants_portal_host(void) {
    static char host[256] = "";
    if (!host[0]) {
        const char *env = getenv("QQ_PORTAL_HOST");
        snprintf(host, sizeof host, "%s", (env && *env) ? env : "q.qq.com");
    }
    return host;
}

/* PoP: qqbot_constants_api_base @ gateway/platforms/qqbot/constants.py:API_BASE */
const char *qqbot_constants_api_base(void) { return "https://api.sgroup.qq.com"; }

/* PoP: qqbot_constants_token_url @ gateway/platforms/qqbot/constants.py:TOKEN_URL */
const char *qqbot_constants_token_url(void) {
    return "https://bots.qq.com/app/getAppAccessToken";
}

/* PoP: qqbot_constants_gateway_url_path @ gateway/platforms/qqbot/constants.py:GATEWAY_URL_PATH */
const char *qqbot_constants_gateway_url_path(void) { return "/gateway"; }

/* PoP: qqbot_constants_onboard_create_path @ gateway/platforms/qqbot/constants.py:ONBOARD_CREATE_PATH */
const char *qqbot_constants_onboard_create_path(void) { return "/lite/create_bind_task"; }

/* PoP: qqbot_constants_onboard_poll_path @ gateway/platforms/qqbot/constants.py:ONBOARD_POLL_PATH */
const char *qqbot_constants_onboard_poll_path(void) { return "/lite/poll_bind_result"; }

/* PoP: qqbot_constants_default_api_timeout @ gateway/platforms/qqbot/constants.py:DEFAULT_API_TIMEOUT */
double qqbot_constants_default_api_timeout(void) { return 30.0; }

/* PoP: qqbot_constants_file_upload_timeout @ gateway/platforms/qqbot/constants.py:FILE_UPLOAD_TIMEOUT */
double qqbot_constants_file_upload_timeout(void) { return 120.0; }

/* PoP: qqbot_constants_reconnect_backoff @ gateway/platforms/qqbot/constants.py:RECONNECT_BACKOFF */
const int *qqbot_constants_reconnect_backoff(int *out_n) {
    static const int steps[] = {2, 5, 10, 30, 60};
    if (out_n) *out_n = 5;
    return steps;
}

/* PoP: qqbot_constants_max_reconnect_attempts @ gateway/platforms/qqbot/constants.py:MAX_RECONNECT_ATTEMPTS */
int qqbot_constants_max_reconnect_attempts(void) { return 100; }

/* PoP: qqbot_constants_rate_limit_delay @ gateway/platforms/qqbot/constants.py:RATE_LIMIT_DELAY */
int qqbot_constants_rate_limit_delay(void) { return 60; }

/* PoP: qqbot_constants_quick_disconnect_threshold @ gateway/platforms/qqbot/constants.py:QUICK_DISCONNECT_THRESHOLD */
double qqbot_constants_quick_disconnect_threshold(void) { return 5.0; }

/* PoP: qqbot_constants_max_quick_disconnect_count @ gateway/platforms/qqbot/constants.py:MAX_QUICK_DISCONNECT_COUNT */
int qqbot_constants_max_quick_disconnect_count(void) { return 3; }
