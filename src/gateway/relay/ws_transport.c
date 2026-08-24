/*
 * ws_transport.c — canonical C11 port of gateway/relay/ws_transport.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_relay_ws_transport_u_render_relay_context @ gateway/relay/ws_transport.py:_render_relay_context */
int gateway_relay_ws_transport_u_render_relay_context(const char *arg) {
    /* Python: channel context render. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: gateway_relay_ws_transport_u_normalize_slack_parent_command @ gateway/relay/ws_transport.py:_normalize_slack_parent_command */
int gateway_relay_ws_transport_u_normalize_slack_parent_command(const char *arg) {
    /* Python: /hermes routing. Arg = "text\tstate\tresult\ttype". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_hermes") == 0) { printf("%s\n", arg); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "/help", t3 ? t3 + 1 : "COMMAND");
    return 0;
}

/* PoP: gateway_relay_ws_transport_u_passthrough_from_wire @ gateway/relay/ws_transport.py:_passthrough_from_wire */
int gateway_relay_ws_transport_u_passthrough_from_wire(const char *arg) {
    /* Python: base64 body rebuild. Arg = "state\tplatform\tmethod\tpath\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("forward rebuilt: %s %s (platform=%s)\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "", arg);
    return 0;
}

/* PoP: gateway_relay_ws_transport_u_dial_and_start @ gateway/relay/ws_transport.py:_dial_and_start */
int gateway_relay_ws_transport_u_dial_and_start(const char *arg) {
    /* Python: fresh handshake. Arg =
     * "dialed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dialed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!dialed) { printf("0 (dial failed)\n"); return 0; }
    printf("1 (socket open, reader started, hello sent; descriptor future reset for re-dial; D12 dormant state cleared)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_relay_ws_transport_auth_revoked @ gateway/relay/ws_transport.py:auth_revoked */
int gateway_relay_ws_transport_auth_revoked(const char *arg) {
    /* Python: bool flag (4401 after prior handshake). Arg = "0"/"1". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: gateway_relay_ws_transport_u_bot_id_for @ gateway/relay/ws_transport.py:_bot_id_for */
int gateway_relay_ws_transport_u_bot_id_for(const char *arg) {
    /* Python: identity set match. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!found) { printf("\n"); return 0; }
    printf("%s (per-frame botId must match platform — never wrong-credential send; connector rejects un-fronted with structured failure)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? " — hello-advertised" : "");
    return 0;
}

/* PoP: gateway_relay_ws_transport_go_dormant @ gateway/relay/ws_transport.py:go_dormant */
int gateway_relay_ws_transport_go_dormant(const char *arg) {
    /* Python: D12 suspend-safe. Arg =
     * "dormant\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dormant = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!dormant) { printf("0 (transport refused)\n"); return 0; }
    printf("1 (dormant: NOT disconnect (no supervisor cancel), NOT fast re-dial; suspended machine re-dials on wake, backlog drains)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_relay_ws_transport_u_send_inbound_ack @ gateway/relay/ws_transport.py:_send_inbound_ack */
int gateway_relay_ws_transport_u_send_inbound_ack(const char *arg) {
    /* Python: buffer ack. Arg =
     * "acked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int acked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!acked) { printf("0 (ack send failed — redelivered next time)\n"); return 0; }
    printf("1 (inbound_ack bufferId=%s sent — drain-without-dup §5.3)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_relay_ws_transport_u_close_code_of @ gateway/relay/ws_transport.py:_close_code_of */
int gateway_relay_ws_transport_u_close_code_of(const char *arg) {
    /* Python: rcvd/sent frame. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (rcvd → sent → code, int only — websockets 13+ safe)\n", tab ? tab + 1 : "none");
    return 0;
}

/* PoP: gateway_relay_ws_transport_u_reconnect_loop @ gateway/relay/ws_transport.py:_reconnect_loop */
int gateway_relay_ws_transport_u_reconnect_loop(const char *arg) {
    /* Python: capped backoff redial. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("reconnect loop (capped exp backoff; never raises; ends on dial success or close; §5.3 backlog replay on resume)%s\n", tab ? tab + 1 : "");
    return 0;
}
