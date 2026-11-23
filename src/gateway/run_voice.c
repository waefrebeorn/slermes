/*
 * run_voice.c - Voice-channel key helpers (C11 port).
 *
 * PoP: w6_runvoice_5cc499_voice_key @ gateway/run_voice.py:_voice_key
 * PoP: w6_runvoice_5cc499_is_legacy_voice_key @ gateway/run_voice.py:_is_legacy_voice_key
 *
 * The platform-name set mirrors gateway.config.Platform values from the live
 * module (snapshot of the 22 values reported by Platform at port time).
 */
#include "noc_w6_runvoice_5cc499.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static const char *py_strip(const char *s) {
    static char buf[4100];
    size_t n = 0;
    const char *a = s ? s : "";
    while (*a && py_space((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && py_space((unsigned char)b[-1])) b--;
    while (a < b && n < sizeof(buf) - 1) buf[n++] = *a++;
    buf[n] = '\0';
    return buf;
}

static const char *const PLATFORMS[] = {
    "api_server", "bluebubbles", "dingtalk", "discord", "email", "feishu",
    "local", "matrix", "mattermost", "msgraph_webhook", "qqbot", "relay",
    "signal", "slack", "sms", "telegram", "webhook", "wecom",
    "wecom_callback", "weixin", "whatsapp", "whatsapp_cloud"
};
static int is_platform_name(const char *s) {
    if (!s) return 0;
    for (size_t i = 0; i < sizeof(PLATFORMS) / sizeof(PLATFORMS[0]); i++)
        if (!strcmp(s, PLATFORMS[i])) return 1;
    return 0;
}

char *w6_runvoice_5cc499_voice_key(const char *platform_value, const char *chat_id, const char *profile) {
    const char *pv = platform_value ? platform_value : "";
    const char *cid = chat_id ? chat_id : "";
    char base[4100];
    snprintf(base, sizeof base, "%s:%s", pv, cid);
    const char *pr = py_strip(profile);
    if (!*pr || !strcmp(pr, "default")) return strdup(base);
    size_t len = strlen(pr) + 1 + strlen(base) + 1;
    char *out = malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "%s:%s", pr, base);
    return out;
}

int w6_runvoice_5cc499_is_legacy_voice_key(const char *key) {
    const char *s = key ? key : "";
    const char *c1 = strchr(s, ':');
    char p0[1024];
    size_t n = c1 ? (size_t)(c1 - s) : strlen(s);
    if (n >= sizeof p0) n = sizeof p0 - 1;
    memcpy(p0, s, n); p0[n] = 0;
    if (is_platform_name(p0)) return 0;
    if (c1) {
        const char *c2 = strchr(c1 + 1, ':');
        if (c2) {
            char p1[1024];
            size_t m = (size_t)(c2 - (c1 + 1));
            if (m >= sizeof p1) m = sizeof p1 - 1;
            memcpy(p1, c1 + 1, m); p1[m] = 0;
            if (is_platform_name(p1)) return 0;
        }
    }
    return 1;
}
void w6_runvoice_5cc499_free_str(char *s) { free(s); }

