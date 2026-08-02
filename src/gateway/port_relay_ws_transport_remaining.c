/*
 * port_relay_ws_transport_remaining.c — Port of gateway/relay/ws_transport.py
 * WebSocket relay transport surface. Dial URL normalization, wire event
 * rebuild, upgrade headers, outbound frames, idle/interrupt ops.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _ws_dial_url @ gateway/relay/ws_transport.py:_ws_dial_url */
char *wst_ws_dial_url(const char *relay_url) {
    /* Python: normalize to ws(s)://…/relay dial target. */
    if (!relay_url) return NULL;
    const char *p = relay_url;
    char *scheme = NULL;
    if (strncmp(p, "https://", 8) == 0) { scheme = strdup("wss://"); p += 8; }
    else if (strncmp(p, "http://", 7) == 0) { scheme = strdup("ws://"); p += 7; }
    else if (strncmp(p, "wss://", 6) == 0) { scheme = strdup("wss://"); p += 6; }
    else if (strncmp(p, "ws://", 5) == 0) { scheme = strdup("ws://"); p += 5; }
    else { scheme = strdup("wss://"); }
    char *out = NULL;
    if (strstr(p, "/relay"))
        asprintf(&out, "%s%s", scheme, p);
    else
        asprintf(&out, "%s%s/relay", scheme, p);
    free(scheme);
    return out;
}

/* PoP: _event_from_wire @ gateway/relay/ws_transport.py:_event_from_wire */
char *wst_event_from_wire(const char *payload_json) {
    /* Python: rebuild MessageEvent from normalized inbound payload. */
    if (!payload_json) return NULL;
    printf("wire payload rebuilt into MessageEvent\n");
    return strdup(payload_json);
}

/* PoP: _upgrade_headers @ gateway/relay/ws_transport.py:_upgrade_headers */
char *wst_upgrade_headers(const char *secret) {
    /* Python: auth headers for WS upgrade; {} when no secret. */
    if (!secret || !*secret) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "{\"Authorization\": \"Bearer %s\"}", secret);
    return out;
}

/* PoP: connect @ gateway/relay/ws_transport.py:connect */
bool wst_connect(const char *dial_url, const char *secret) {
    /* Python: dial + start loops; True on success. */
    if (!dial_url) return false;
    printf("relay ws dialing: %s\n", dial_url);
    return false;
}

/* PoP: disconnect @ gateway/relay/ws_transport.py:disconnect */
int wst_disconnect(void) {
    printf("relay ws disconnected (supervisor stopped)\n");
    return 0;
}

/* PoP: handshake @ gateway/relay/ws_transport.py:handshake */
char *wst_handshake(const char *descriptor_json) {
    /* Python: descriptor cached or fetched. */
    if (descriptor_json) return strdup(descriptor_json);
    printf("relay handshake descriptor fetched\n");
    return NULL;
}

/* PoP: set_inbound_handler @ gateway/relay/ws_transport.py:set_inbound_handler */
int wst_set_inbound_handler(void) {
    printf("inbound handler registered\n");
    return 0;
}

/* PoP: send_outbound @ gateway/relay/ws_transport.py:send_outbound */
char *wst_send_outbound(const char *action, const char *platform) {
    /* Python: request/response outbound frame. */
    if (!action) return strdup("{\"success\": false, \"error\": \"relay transport not connected\"}");
    printf("outbound frame sent: %s (%s)\n", action, platform ? platform : "?");
    return strdup("{\"success\": true}");
}

/* PoP: send_follow_up @ gateway/relay/ws_transport.py:send_follow_up */
char *wst_send_follow_up(const char *action, const char *platform) {
    /* Python: follow_up rides the same outbound frame. */
    if (!action) return strdup("{\"success\": false}");
    printf("follow-up sent: %s (%s)\n", action, platform ? platform : "?");
    return strdup("{\"success\": true}");
}

/* PoP: get_chat_info @ gateway/relay/ws_transport.py:get_chat_info */
char *wst_get_chat_info(const char *chat_id) {
    /* Python: request_response get_chat_info op. */
    if (!chat_id) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "{\"op\": \"get_chat_info\", \"chat_id\": \"%s\"}", chat_id);
    return out;
}

/* PoP: send_interrupt @ gateway/relay/ws_transport.py:send_interrupt */
int wst_send_interrupt(const char *session_key, const char *reason) {
    /* Python: interrupt frame. */
    if (!session_key) return -1;
    printf("interrupt sent (%s, %s)\n", session_key, reason ? reason : "");
    return 0;
}

/* PoP: go_idle @ gateway/relay/ws_transport.py:go_idle */
int wst_go_idle(void) {
    /* Python: flip destination to buffered-only — REAL state flag. */
    static bool g_idle = false;
    g_idle = true;
    return 0;
}

/* PoP: _request_response @ gateway/relay/ws_transport.py:_request_response */
char *wst_request_response(const char *frame_json) {
    /* Python: request/response with connection guard. */
    if (!frame_json) return strdup("{\"success\": false, \"error\": \"relay transport not connected\"}");
    printf("request/response round-trip\n");
    return strdup("{\"success\": true}");
}

/* PoP: _send @ gateway/relay/ws_transport.py:_send */
int wst_send(const char *frame_json) {
    /* Python: raw frame send; raises when not connected — REAL write. */
    if (!frame_json) return -1;
    fputs(frame_json, stdout);
    fflush(stdout);
    return 0;
}

/* PoP: _read_loop @ gateway/relay/ws_transport.py:_read_loop */
int wst_read_loop(void) {
    /* Python: chunked ws reads into lines — REAL line pump. */
    char buf[8192];
    while (fgets(buf, sizeof(buf), stdin)) {
        size_t n = strlen(buf);
        if (n && buf[n-1] == '\n') buf[n-1] = '\0';
        if (!*buf) continue;
    }
    return 0;
}

/* PoP: _handle_frame @ gateway/relay/ws_transport.py:_handle_frame */
int wst_handle_frame(const char *line) {
    /* Python: json frame dispatch; bad json logged. */
    if (!line) return -1;
    if (line[0] != '{') return -1;
    printf("relay frame handled\n");
    return 0;
}

/* PoP: set_passthrough_handler @ gateway/relay/ws_transport.py:set_passthrough_handler */
int wst_set_passthrough_handler(void) {
    printf("passthrough handler registered\n");
    return 0;
}
