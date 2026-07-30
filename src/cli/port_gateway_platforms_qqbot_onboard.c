/*
 * port_gateway_platforms_qqbot_onboard.c — C port of
 * gateway/platforms/qqbot/onboard.py
 *
 * QQBot scan-to-configure (QR code onboard) module. Real HTTP calls to the
 * q.qq.com create_bind_task / poll_bind_result APIs (faithful to the Python).
 */

#include "hermes_logger.h"
#include "libjson/json.h"
#include "libhttp/http.h"
#include "libbase64/base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define QQ_PORTAL_HOST_ENV "QQ_PORTAL_HOST"
#define QQ_ONBOARD_CREATE_PATH "/lite/create_bind_task"
#define QQ_ONBOARD_POLL_PATH "/lite/poll_bind_result"
#define QQ_ONBOARD_API_TIMEOUT 10
#define MAX_REFRESHES 3
#define ONBOARD_POLL_INTERVAL 2

/* PoP: qqbot_onboard__render_qr @ gateway/platforms/qqbot/onboard.py:_render_qr */
/* No qrcode library in the C tree; return 0 so the caller prints the URL. */
int qqbot_onboard__render_qr(const char *url)
{
    if (!url || !url[0]) return 0;
    hermes_log(LOG_DEBUG, "qqbot_onboard", "QR rendering not available, use URL: %s", url);
    return 0;
}

/* PoP: qqbot_onboard__create_bind_task @ gateway/platforms/qqbot/onboard.py:_create_bind_task */
/* Real POST to q.qq.com create_bind_task. Generates the AES key client-side
 * (base64 of 32 random bytes) and returns the server-issued task_id. */
int qqbot_onboard__create_bind_task(char *task_id, size_t task_id_len,
                                      char *aes_key, size_t key_len)
{
    if (!task_id || !aes_key) return -1;

    /* Generate the bind key: base64(32 random bytes) from /dev/urandom. */
    unsigned char raw[32];
    int ur = open("/dev/urandom", O_RDONLY);
    if (ur < 0 || read(ur, raw, sizeof(raw)) != (ssize_t)sizeof(raw)) {
        if (ur >= 0) close(ur);
        return -1;
    }
    close(ur);
    char *key_b64 = base64_encode(raw, sizeof(raw));
    if (!key_b64) return -1;
    snprintf(aes_key, key_len, "%s", key_b64);
    free(key_b64);

    const char *host = getenv(QQ_PORTAL_HOST_ENV);
    if (!host) host = "q.qq.com";

    char url[512];
    snprintf(url, sizeof(url), "https://%s%s", host, QQ_ONBOARD_CREATE_PATH);

    char body[256];
    snprintf(body, sizeof(body), "{\"key\":\"%s\"}", aes_key);

    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: application/json\r\nAccept: application/json\r\nUser-Agent: QQBotAdapter/1.0.0 (Hermes/slermes)");

    http_t *http = http_new(QQ_ONBOARD_API_TIMEOUT);
    if (!http) return -1;

    http_resp_t *res = http_request(http, HTTP_POST, url, headers, body, strlen(body));
    int rc = -1;
    if (res && res->status >= 200 && res->status < 300 && res->body) {
        json_t *doc = json_parse(res->body, NULL);
        if (doc && doc->type == JSON_OBJECT) {
            int retcode = (int)json_get_num(doc, "retcode", -1);
            if (retcode == 0) {
                json_t *data = json_obj_get(doc, "data");
                const char *tid = data ? json_get_str(data, "task_id", NULL) : NULL;
                if (tid && tid[0]) {
                    snprintf(task_id, task_id_len, "%s", tid);
                    rc = 0;
                }
            } else {
                const char *msg = json_get_str(doc, "msg", "create_bind_task failed");
                hermes_log(LOG_ERROR, "qqbot_onboard", "create_bind_task: %s", msg);
            }
        }
        if (doc) json_free(doc);
    }
    if (res) http_resp_free(res);
    http_free(http);

    if (rc == 0)
        hermes_log(LOG_INFO, "qqbot_onboard", "Created bind task: %s", task_id);
    return rc;
}

/* PoP: qqbot_onboard__poll_bind_result @ gateway/platforms/qqbot/onboard.py:_poll_bind_result */
/* Real POST to poll_bind_result; returns status + bot_appid/secret/openid. */
int qqbot_onboard__poll_bind_result(const char *task_id, char *bot_appid, size_t appid_len,
                                      char *encrypted_secret, size_t secret_len,
                                      char *user_openid, size_t openid_len)
{
    if (!task_id || !task_id[0]) return -1;
    if (bot_appid && appid_len) bot_appid[0] = '\0';
    if (encrypted_secret && secret_len) encrypted_secret[0] = '\0';
    if (user_openid && openid_len) user_openid[0] = '\0';

    const char *host = getenv(QQ_PORTAL_HOST_ENV);
    if (!host) host = "q.qq.com";

    char url[512];
    snprintf(url, sizeof(url), "https://%s%s", host, QQ_ONBOARD_POLL_PATH);

    char body[256];
    snprintf(body, sizeof(body), "{\"task_id\":\"%s\"}", task_id);

    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: application/json\r\nAccept: application/json\r\nUser-Agent: QQBotAdapter/1.0.0 (Hermes/slermes)");

    http_t *http = http_new(QQ_ONBOARD_API_TIMEOUT);
    if (!http) return -1;

    int status = 1; /* PENDING default */
    http_resp_t *res = http_request(http, HTTP_POST, url, headers, body, strlen(body));
    if (res && res->status >= 200 && res->status < 300 && res->body) {
        json_t *doc = json_parse(res->body, NULL);
        if (doc && doc->type == JSON_OBJECT) {
            int retcode = (int)json_get_num(doc, "retcode", -1);
            if (retcode == 0) {
                json_t *d = json_obj_get(doc, "data");
                if (d) {
                    status = (int)json_get_num(d, "status", 1);
                    const char *appid = json_get_str(d, "bot_appid", NULL);
                    const char *secret = json_get_str(d, "bot_encrypt_secret", NULL);
                    const char *openid = json_get_str(d, "user_openid", NULL);
                    if (appid && bot_appid) snprintf(bot_appid, appid_len, "%s", appid);
                    if (secret && encrypted_secret) snprintf(encrypted_secret, secret_len, "%s", secret);
                    if (openid && user_openid) snprintf(user_openid, openid_len, "%s", openid);
                }
            } else {
                const char *msg = json_get_str(doc, "msg", "poll_bind_result failed");
                hermes_log(LOG_WARNING, "qqbot_onboard", "poll_bind_result: %s", msg);
            }
        }
        if (doc) json_free(doc);
    }
    if (res) http_resp_free(res);
    http_free(http);

    hermes_log(LOG_DEBUG, "qqbot_onboard", "Polled bind result for %s: status=%d", task_id, status);
    return status;
}

/* PoP: qqbot_onboard_build_connect_url @ gateway/platforms/qqbot/onboard.py:build_connect_url */
char *qqbot_onboard_build_connect_url(const char *task_id)
{
    if (!task_id || !task_id[0]) return strdup("");
    char *url = malloc(512);
    if (url)
        snprintf(url, 512, "https://q.qq.com/qqbot/openclaw/connect.html?task_id=%s&_wv=2&source=hermes", task_id);
    return url;
}

/* PoP: qqbot_onboard_qr_register @ gateway/platforms/qqbot/onboard.py:qr_register */
int qqbot_onboard_qr_register(int timeout_seconds)
{
    if (timeout_seconds <= 0) timeout_seconds = 600;

    char task_id[128];
    char aes_key[64];
    char url[512];
    char *connect_url = NULL;

    printf("\n  QQBot QR Registration\n  ======================\n\n");

    for (int refresh = 0; refresh <= MAX_REFRESHES; refresh++) {
        if (qqbot_onboard__create_bind_task(task_id, sizeof(task_id), aes_key, sizeof(aes_key)) != 0) {
            fprintf(stderr, "  Failed to create bind task\n");
            return -1;
        }

        connect_url = qqbot_onboard_build_connect_url(task_id);
        if (connect_url) {
            snprintf(url, sizeof(url), "%s", connect_url);
            free(connect_url);
        }

        if (!qqbot_onboard__render_qr(url)) {
            printf("  Open this URL in QQ on your phone:\n  %s\n", url);
        }
        printf("\n");

        time_t deadline = time(NULL) + timeout_seconds;
        while (time(NULL) < deadline) {
            char bot_appid[128] = {0};
            char encrypted_secret[256] = {0};
            char user_openid[128] = {0};

            int status = qqbot_onboard__poll_bind_result(task_id,
                                                          bot_appid, sizeof(bot_appid),
                                                          encrypted_secret, sizeof(encrypted_secret),
                                                          user_openid, sizeof(user_openid));
            if (status == 2) { /* COMPLETED */
                printf("  QR scan complete! (App ID: %s)\n", bot_appid);
                hermes_log(LOG_INFO, "qqbot_onboard", "QR registration successful");
                return 0;
            }
            if (status == 3) { /* EXPIRED */
                if (refresh >= MAX_REFRESHES) {
                    fprintf(stderr, "  QR code expired %d times - giving up\n", MAX_REFRESHES);
                    return -1;
                }
                printf("  QR code expired, refreshing... (%d/%d)\n", refresh + 1, MAX_REFRESHES);
                break;
            }
            sleep(ONBOARD_POLL_INTERVAL);
        }
    }

    fprintf(stderr, "  QR registration timed out or failed\n");
    return -1;
}
