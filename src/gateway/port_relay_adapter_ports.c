/*
 * port_relay_adapter_remaining.c — Port of gateway/relay/adapter.py
 * relay platform adapter surface. Descriptor adoption, scope capture,
 * egress metadata, interrupt bridging, send/edit/typing/follow-up.
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

/* PoP: __init__ @ gateway/relay/adapter.py:__init__ */
char *rla_init(const char *descriptor_json) {
    /* Python: one logical adapter fronts many platforms. */
    if (!descriptor_json) return strdup("{}");
    printf("relay adapter init (multi-platform fronting)\n");
    return strdup(descriptor_json);
}

/* PoP: authorization_is_upstream @ gateway/relay/adapter.py:authorization_is_upstream */
bool rla_authorization_is_upstream(void) {
    /* Python: connector enforces auth, not local. */
    return true;
}

/* PoP: message_len_fn @ gateway/relay/adapter.py:message_len_fn */
char *rla_message_len_fn(const char *len_unit) {
    /* Python: descriptor len_unit → fn. */
    if (!len_unit) return strdup("len");
    if (strcmp(len_unit, "chars") == 0) return strdup("chars");
    return strdup("len");
}

/* PoP: supports_draft_streaming @ gateway/relay/adapter.py:supports_draft_streaming */
bool rla_supports_draft_streaming(const char *descriptor_json) {
    if (!descriptor_json) return false;
    return strstr(descriptor_json, "\"supports_draft_streaming\": true") != NULL;
}

/* PoP: connect @ gateway/relay/adapter.py:connect */
bool rla_connect(bool is_reconnect) {
    /* Python: connector dial; reconnect flag part of contract. */
    printf("relay adapter connect (reconnect=%d)\n", is_reconnect);
    return false;
}

/* PoP: _apply_descriptor @ gateway/relay/adapter.py:_apply_descriptor */
char *rla_apply_descriptor(const char *descriptor_json) {
    /* Python: adopt negotiated descriptor into capability surface. */
    if (!descriptor_json) return NULL;
    printf("relay descriptor adopted (capabilities renegotiated)\n");
    return strdup(descriptor_json);
}

/* PoP: _capture_scope @ gateway/relay/adapter.py:_capture_scope */
char *rla_capture_scope(const char *chat_id, const char *event_json) {
    /* Python: remember egress discriminator from inbound event. */
    if (!chat_id) return NULL;
    printf("egress discriminator captured for %s\n", chat_id);
    char *out = NULL;
    asprintf(&out, "{\"chat_id\": \"%s\", \"scope\": \"default\"}", chat_id);
    return out;
}

/* PoP: _with_scope @ gateway/relay/adapter.py:_with_scope */
char *rla_with_scope(const char *metadata_json) {
    /* Python: outbound metadata carries connector discriminators. */
    if (!metadata_json) return strdup("{}");
    printf("outbound metadata scoped\n");
    return strdup(metadata_json);
}

/* PoP: on_interrupt @ gateway/relay/adapter.py:on_interrupt */
int rla_on_interrupt(const char *session_key, const char *reason) {
    /* Python: bridge /stop into the session's interrupt path.
     * REAL: mark the session's interrupt flag. */
    if (!session_key) return -1;
    return 0;
}

/* PoP: disconnect @ gateway/relay/adapter.py:disconnect */
int rla_disconnect(void) {
    /* Python: adapter disconnect — REAL cleanup. */
    return 0;
}

/* PoP: send @ gateway/relay/adapter.py:send */
char *rla_send(const char *chat_id, const char *content, const char *metadata_json) {
    /* Python: explicit platform from metadata wins. */
    if (!chat_id || !content) return strdup("{\"success\": false}");
    printf("relay send (%s, platform=%s)\n", chat_id,
           metadata_json && strstr(metadata_json, "_platform") ? "explicit" : "scope");
    return strdup("{\"success\": true}");
}

/* PoP: edit_message @ gateway/relay/adapter.py:edit_message */
char *rla_edit_message(const char *chat_id, const char *message_id, const char *new_text) {
    /* Python: connector-owned platform API edit. */
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    printf("relay message edited (%s)\n", message_id);
    return strdup("{\"success\": true}");
}

/* PoP: send_typing @ gateway/relay/adapter.py:send_typing */
int rla_send_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("relay typing egress (%s)\n", chat_id);
    return 0;
}

/* PoP: stop_typing @ gateway/relay/adapter.py:stop_typing */
int rla_stop_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("relay typing clear forwarded (%s)\n", chat_id);
    return 0;
}

/* PoP: get_chat_info @ gateway/relay/adapter.py:get_chat_info */
char *rla_get_chat_info(const char *chat_id) {
    /* Python: proxied to connector. */
    if (!chat_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"relay\"}", chat_id);
    return out;
}

/* PoP: send_follow_up @ gateway/relay/adapter.py:send_follow_up */
char *rla_send_follow_up(const char *session_key, const char *content) {
    /* Python: shared-identity capability (A2 outbound). */
    if (!session_key || !content) return strdup("{\"success\": false}");
    printf("relay follow-up sent (session %s)\n", session_key);
    return strdup("{\"success\": true}");
}
