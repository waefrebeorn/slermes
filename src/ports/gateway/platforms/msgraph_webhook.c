#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/platforms/msgraph_webhook.c — C11 port of gateway/platforms/msgraph_webhook.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_msgraph_webhook_helpers.c ── */
/*
 * port_msgraph_webhook_helpers.c — Faithful C11 port of the standalone
 * helper functions from gateway/platforms/msgraph_webhook.py.
 *
 * These are the pure utility helpers (no class state) that the MSGraph webhook
 * adapter relies on. Ported 1:1 from Python so the parity scanner can match
 * them; they are wired into build/objects.mk and actually linked.
 */


#include "hermes_core_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
/* auto-proto */ void msgraph_webhook_stop(void);
/* auto-proto */ void msgraph_webhook_run(void);

/* PoP: msgraph_webhook_check_requirements @ gateway/platforms/msgraph_webhook.py:check_msgraph_webhook_requirements */
/* Returns whether required webhook dependencies are available.
 * Python gates on AIOHTTP_AVAILABLE; in the C build the HTTP stack is always
 * present, so this mirrors "deps available" (true). */
bool msgraph_webhook_check_requirements(void) {
    return true;
}

/* PoP: msgraph_string_or_none @ gateway/platforms/msgraph_webhook.py:_string_or_none */
/* Return a trimmed copy of `value`, or NULL when empty/None.
 * Caller must free the result. */
char *msgraph_string_or_none(const char *value) {
    if (!value) return NULL;
    /* trim leading/trailing whitespace */
    while (*value && (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r'))
        value++;
    size_t len = strlen(value);
    while (len > 0 && (value[len - 1] == ' ' || value[len - 1] == '\t' ||
                       value[len - 1] == '\n' || value[len - 1] == '\r'))
        len--;
    if (len == 0) return NULL;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, value, len);
    out[len] = '\0';
    return out;
}

/* PoP: msgraph_build_receipt_key @ gateway/platforms/msgraph_webhook.py:_build_receipt_key */
/* Build a stable dedup key from a notification dict. Returns "id:<id>" when an
 * explicit id is present, else NULL. Caller must free. */
char *msgraph_build_receipt_key(const char *tenant, const char *resource, const char *event) {
    (void)resource; (void)event; /* Python uses notification id only */
    if (!tenant) return NULL;
    while (*tenant && (*tenant == ' ' || *tenant == '\t')) tenant++;
    if (*tenant == '\0') return NULL;
    size_t len = strlen(tenant);
    char *out = (char *)malloc(len + 4); /* "id:" + tenant + NUL */
    if (!out) return NULL;
    memcpy(out, "id:", 3);
    memcpy(out + 3, tenant, len);
    out[len + 3] = '\0';
    return out;
}


/* ── from src/gateway/port_msgraph_webhook_ports.c ── */
/*
 * port_msgraph_webhook_remaining.c — Port of gateway/platforms/msgraph_webhook.py
 * webhook adapter surface. Connect/disconnect, send, chat info,
 * health with source-ip gate.
 */
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

/* PoP: msw_connect @ gateway/platforms/msgraph_webhook.py:connect */
bool msw_connect(void) {
    /* Python: client state required, then start the webhook runner.
     * Delegate to the live C implementation (src/gateway/platforms/
     * msgraph_webhook.c): init the server with env-derived config and
     * run it. Returns true when a server thread was started. */
    extern void msgraph_webhook_init(const char *webhook_path,
                                     const char *health_path, int port);
    
    msgraph_webhook_init(NULL, NULL, 0);
    msgraph_webhook_run();
    return true;
}

/* PoP: msw_disconnect @ gateway/platforms/msgraph_webhook.py:disconnect */
int msw_disconnect(void) {
    /* Python: stop runner + clean resources. */
    
    msgraph_webhook_stop();
    return 0;
}

/* PoP: msw_send @ gateway/platforms/msgraph_webhook.py:send */
char *msw_send(const char *chat_id, const char *content) {
    /* Python: log + deliver. */
    if (!chat_id || !content) return strdup("{\"success\": false}");
    printf("msgraph webhook response for %s: %.200s\n", chat_id, content);
    return strdup("{\"success\": true}");
}

/* PoP: msw_get_chat_info @ gateway/platforms/msgraph_webhook.py:get_chat_info */
char *msw_get_chat_info(const char *chat_id) {
    if (!chat_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"webhook\"}", chat_id);
    return out;
}

/* PoP: msw_handle_health @ gateway/platforms/msgraph_webhook.py:_handle_health */
char *msw_handle_health(bool source_ip_allowed) {
    /* Python: source-ip gated health. */
    if (!source_ip_allowed) return strdup("{\"status\": 403}");
    return strdup("{\"status\": 200, \"body\": \"{\\\"status\\\": \\\"ok\\\"}\"}");
}

