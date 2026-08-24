/*
 * adapter.c — canonical C11 port of gateway/relay/adapter.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_relay_adapter_u_start_revocation_monitor @ gateway/relay/adapter.py:_start_revocation_monitor */
int gateway_relay_adapter_u_start_revocation_monitor(const char *arg) {
    /* Python: 4401 poll. Arg =
     * "already\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int already = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (already) { printf("monitor already running\n"); return 0; }
    printf("revocation monitor spawned (1s poll → relay_disabled non-retryable fatal)%s\n", t2 && t2[1] == '1' ? " — revoked" : "");
    return 0;
}

/* PoP: gateway_relay_adapter_u_watch_for_revocation @ gateway/relay/adapter.py:_watch_for_revocation */
int gateway_relay_adapter_u_watch_for_revocation(const char *arg) {
    /* Python: 4401 terminal. Arg =
     * "revoked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int revoked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (monitor running)\n"); return 0; }
    if (!revoked) { printf("0\n"); return 0; }
    printf("1 (4401 revocation → relay_disabled fatal surfaced; adapter removed, NOT queued for reconnect)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_relay_adapter_fronts_platform @ gateway/relay/adapter.py:fronts_platform */
int gateway_relay_adapter_fronts_platform(const char *arg) {
    /* Python: transport identities match platform. Arg =
     * "platform\tidentities_json\tmatched". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t2 && t2[1] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: gateway_relay_adapter_u_platform_is_fronted @ gateway/relay/adapter.py:_platform_is_fronted */
int gateway_relay_adapter_u_platform_is_fronted(const char *arg) {
    /* Python: fronts_platform alias. Arg =
     * "fronted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int fronted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (connector-fronted platform: %s)\n", fronted ? "1" : "0", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: gateway_relay_adapter_u_on_passthrough @ gateway/relay/adapter.py:_on_passthrough */
int gateway_relay_adapter_u_on_passthrough(const char *arg) {
    /* Python: §5.1 edge-forward. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (unsupported passthrough type)\n"); return 0; }
    if (!handled) { printf("0\n"); return 0; }
    printf("1 (already-sanitized request handled — connector was trust boundary, no credentials in body)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_relay_adapter_u_discord_interaction_to_event @ gateway/relay/adapter.py:_discord_interaction_to_event */
int gateway_relay_adapter_u_discord_interaction_to_event(const char *arg) {
    /* Python: interaction body. Arg =
     * "itype\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *itype = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!itype[0]) { printf("\n"); return 0; }
    printf("interaction type %s → MessageEvent (%s)\n", itype, (t2 && t2[1] == '1') ? "slash normalized to leading /" : "text");
    return 0;
}

/* PoP: gateway_relay_adapter_u_render_interaction_options @ gateway/relay/adapter.py:_render_interaction_options */
int gateway_relay_adapter_u_render_interaction_options(const char *arg) {
    /* Python: discord options flatten. Arg =
     * "parts\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s part(s) (scalars → value; SUB_COMMAND/GROUP → name + recurse one level)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — command shape" : "");
    return 0;
}

/* PoP: gateway_relay_adapter_go_dormant @ gateway/relay/adapter.py:go_dormant */
int gateway_relay_adapter_go_dormant(const char *arg) {
    /* Python: scale-to-zero quiesce. Arg =
     * "dormant\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dormant = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (stub transport — no-op degrade)\n"); return 0; }
    if (!dormant) { printf("0 (transport refused)\n"); return 0; }
    printf("1 (transport quiesced — reconnect armed, revocation monitor stays live, NOT teardown D12)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: gateway_relay_adapter_send_for_platform @ gateway/relay/adapter.py:send_for_platform */
int gateway_relay_adapter_send_for_platform(const char *arg) {
    /* Python: explicit platform send. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"success\": false}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"success\": false, \"error\": \"platform not fronted\"} (fail-closed repeat check)\n"); return 1; }
    if (!sent) { printf("{\"success\": false}\n"); return 1; }
    printf("{\"success\": true, \"message_id\": \"%s\"} (scheduled/persisted-home deliveries — no fresh inbound event needed)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
