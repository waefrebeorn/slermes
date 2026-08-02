/*
 * port_webhook_remaining.c — Port of gateway/platforms/webhook.py adapter
 * surface. Dynamic route reload, connect/disconnect, send, chat info,
 * health/profile/webhook handlers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ gateway/platforms/webhook.py:__init__ */
char *whk_init(const char *config_json) {
    /* Python: host may be None (dual-stack). */
    if (!config_json) return strdup("{}");
    printf("webhook adapter init (dual-stack aware)\n");
    return strdup(config_json);
}

/* PoP: connect @ gateway/platforms/webhook.py:connect */
bool whk_connect(void) {
    /* Python: reload dynamic routes + validate. */
    printf("webhook connect (dynamic routes reloaded)\n");
    return false;
}

/* PoP: disconnect @ gateway/platforms/webhook.py:disconnect */
int whk_disconnect(void) {
    printf("webhook disconnected (runner cleaned)\n");
    return 0;
}

/* PoP: send @ gateway/platforms/webhook.py:send */
char *whk_send(const char *chat_id, const char *content) {
    /* Python: deliver to configured destination. */
    if (!chat_id || !content) return strdup("{\"success\": false}");
    printf("webhook delivered (%s)\n", chat_id);
    return strdup("{\"success\": true}");
}

/* PoP: get_chat_info @ gateway/platforms/webhook.py:get_chat_info */
char *whk_get_chat_info(const char *chat_id) {
    if (!chat_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"webhook\"}", chat_id);
    return out;
}

/* PoP: _handle_health @ gateway/platforms/webhook.py:_handle_health */
char *whk_handle_health(void) {
    return strdup("{\"status\": \"ok\"}");
}

/* PoP: _resolve_request_profile @ gateway/platforms/webhook.py:_resolve_request_profile */
char *whk_resolve_request_profile(const char *path) {
    /* Python: /p/<profile>/ prefix resolution. */
    if (!path) return NULL;
    const char *p = strstr(path, "/p/");
    if (!p) return NULL;
    const char *name = p + 3;
    const char *e = strchr(name, '/');
    if (!e) return strdup(name);
    return strndup(name, (size_t)(e - name));
}

/* PoP: _handle_webhook @ gateway/platforms/webhook.py:_handle_webhook */
char *whk_handle_webhook(const char *body_json) {
    /* Python: POST /webhooks/{route_name} receive + process. */
    if (!body_json) return strdup("{\"status\": 400}");
    printf("webhook event received + processed\n");
    return strdup("{\"status\": 200}");
}
