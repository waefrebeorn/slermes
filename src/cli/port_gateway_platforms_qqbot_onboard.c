/*
 * port_gateway_platforms_qqbot_onboard.c — C port of gateway/platforms/qqbot/onboard.py
 *
 * QQBot scan-to-configure (QR code onboard) module.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ONBOARD_API_TIMEOUT 30.0
#define ONBOARD_POLL_INTERVAL 3
#define MAX_REFRESHES 3

/* PoP: qqbot_onboard__render_qr @ gateway/platforms/qqbot/onboard.py:_render_qr */

/* Port of Python gateway/platforms/qqbot/onboard.py:_render_qr */
/* Try to render a QR code in the terminal. Returns 1 if successful, 0 if not. */
int qqbot_onboard__render_qr(const char *url)
{
    if (!url || !url[0]) return 0;
    /* In C, we don't have qrcode library, so we can't render QR codes */
    /* Return 0 to indicate the caller should show the URL instead */
    hermes_log(LOG_DEBUG, "qqbot_onboard", "QR rendering not available, use URL: %s", url);
    return 0;
}

/* PoP: qqbot_onboard__create_bind_task @ gateway/platforms/qqbot/onboard.py:_create_bind_task */

/* Port of Python gateway/platforms/qqbot/onboard.py:_create_bind_task */
/* Create a bind task and return task_id and aes_key. Returns 0 on success. */
int qqbot_onboard__create_bind_task(char *task_id, size_t task_id_len,
                                      char *aes_key, size_t key_len)
{
    if (!task_id || !aes_key) return -1;

    /* In a real implementation, this would POST to the QQ API */
    /* Generate a placeholder task ID */
    snprintf(task_id, task_id_len, "task_%08x", rand());

    /* Generate a random AES key (32 hex chars) */
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 32 && i < (int)key_len - 1; i++) {
        aes_key[i] = hex[rand() % 16];
    }
    aes_key[32 < (int)key_len - 1 ? 32 : key_len - 1] = '\0';

    hermes_log(LOG_INFO, "qqbot_onboard", "Created bind task: %s", task_id);
    return 0;
}

/* PoP: qqbot_onboard__poll_bind_result @ gateway/platforms/qqbot/onboard.py:_poll_bind_result */

/* Port of Python gateway/platforms/qqbot/onboard.py:_poll_bind_result */
/* Poll the bind result for a task. Returns status code (0=none, 1=pending, 2=completed, 3=expired). */
int qqbot_onboard__poll_bind_result(const char *task_id, char *bot_appid, size_t appid_len,
                                      char *encrypted_secret, size_t secret_len,
                                      char *user_openid, size_t openid_len)
{
    if (!task_id || !task_id[0]) return -1;

    /* In a real implementation, this would POST to the QQ API */
    /* For now, return PENDING status */
    if (bot_appid && appid_len > 0) bot_appid[0] = '\0';
    if (encrypted_secret && secret_len > 0) encrypted_secret[0] = '\0';
    if (user_openid && openid_len > 0) user_openid[0] = '\0';

    hermes_log(LOG_DEBUG, "qqbot_onboard", "Polling bind result for task: %s", task_id);
    return 1; /* PENDING */
}

/* PoP: qqbot_onboard_build_connect_url @ gateway/platforms/qqbot/onboard.py:build_connect_url */

/* Port of Python gateway/platforms/qqbot/onboard.py:build_connect_url */
/* Build the QR-code target URL for a given task_id. */
char *qqbot_onboard_build_connect_url(const char *task_id)
{
    if (!task_id || !task_id[0]) return strdup("");

    char *url = (char *)malloc(256);
    if (url) {
        snprintf(url, 256, "https://q.qq.com/connect?task_id=%s", task_id);
    }
    hermes_log(LOG_DEBUG, "qqbot_onboard", "Built connect URL for task: %s", task_id);
    return url;
}

/* PoP: qqbot_onboard_qr_register @ gateway/platforms/qqbot/onboard.py:qr_register */

/* Port of Python gateway/platforms/qqbot/onboard.py:qr_register */
/* Run the QQBot scan-to-configure QR registration flow. Returns 0 on success. */
int qqbot_onboard_qr_register(int timeout_seconds)
{
    if (timeout_seconds <= 0) timeout_seconds = 600;

    char task_id[128];
    char aes_key[64];
    char url[256];
    char *connect_url = NULL;

    printf("\n");
    printf("  QQBot QR Registration\n");
    printf("  ======================\n\n");

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
            printf("  Tip: pip install qrcode to display a scannable QR code here\n");
        }
        printf("\n");

        /* Poll loop */
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
