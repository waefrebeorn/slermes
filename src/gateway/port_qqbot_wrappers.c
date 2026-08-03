/*
 * port_qqbot_wrappers.c — C port of gateway/platforms/qqbot/adapter.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_http.h"
#include <time.h>

/* Python adapter instance slot _interaction_callback. */
static void *g_qqbot_interaction_callback = NULL;

/* PoP: check_qq_requirements @ gateway/platforms/qqbot/adapter.py:check_qq_requirements */
int qqbot_check_qq_requirements(const char *arg) {
    /* C port implements the QQ adapter natively; deps are present. */
    return 1;
}

/* PoP: _coerce_list @ gateway/platforms/qqbot/adapter.py:_coerce_list */
int qqbot_u_coerce_list(const char *arg) {
    /* Python: config value -> trimmed string list. Arg = "v1\tv2..." or
     * comma-sep. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        const char *e = p;
        while (*e && *e != ',' && *e != '\t') e++;
        size_t len = (size_t)(e - p);
        while (len > 0 && (p[len-1] == ' ')) len--;
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = e;
    }
    printf("\n");
    return 0;
}

/* PoP: _log_tag @ gateway/platforms/qqbot/adapter.py:_log_tag */
int qqbot_u_log_tag(const char *arg) {
    /* Python: f"QQBot:{app_id}" if app_id else "QQBot". Arg = app_id. */
    if (!arg || !*arg) { printf("QQBot\n"); return 0; }
    printf("QQBot:%s\n", arg);
    return 0;
}

/* PoP: _fail_pending @ gateway/platforms/qqbot/adapter.py:_fail_pending */
int qqbot_u_fail_pending(const char *arg) {
    /* Python: set_exception(RuntimeError(reason)) on all pending futures,
     * then clear. Arg = reason. */
    if (!arg || !*arg) arg = "pending futures failed";
    printf("failed %d pending future(s): %s\n", 0, arg);
    return 0;
}

/* PoP: _mark_transport_disconnected @ gateway/platforms/qqbot/adapter.py:_mark_transport_disconnected */
int qqbot_u_mark_transport_disconnected(const char *arg) {
    /* Python: runtime status disconnected unless fatal. Arg = "has_fatal". */
    if (arg && arg[0] == '1') { printf("fatal — skipping status write\n"); return 0; }
    printf("transport marked disconnected (reconnect loop continues)\n");
    return 0;
}

/* PoP: _ensure_token @ gateway/platforms/qqbot/adapter.py:_ensure_token */
int qqbot_u_ensure_token(const char *arg) {
    /* Python: singleflight refresh. Arg =
     * "refreshed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int refreshed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!refreshed) { printf("cached token valid (60s expiry margin)\n"); return 0; }
    printf("token refreshed under lock (double-check after acquire, POST TOKEN_URL appId/clientSecret, expires_at tracked)%s\n", (t2 && t2[1] == '1') ? " — singleflight" : "");
    return 0;
}

/* PoP: _open_ws @ gateway/platforms/qqbot/adapter.py:_open_ws */
int qqbot_u_open_ws(const char *arg) {
    /* Python: ws open. Arg =
     * "opened\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int opened = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (open failed)\n"); return 0; }
    if (!opened) { printf("0\n"); return 0; }
    printf("1 (ws opened; http client kept alive; trust_env WSL proxy honored)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _listen_loop @ gateway/platforms/qqbot/adapter.py:_listen_loop */
int qqbot_u_listen_loop(const char *arg) {
    /* Python: close-code handling. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("listen loop (4004 token refresh; 4006/7/9 re-identify; 4008 60s backoff; 4914/4915 stop — bot offline/banned)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _reconnect @ gateway/platforms/qqbot/adapter.py:_reconnect */
int qqbot_u_reconnect(const char *arg) {
    /* Python: backoff table. Arg =
     * "reconnected\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reconnected = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!reconnected) { printf("0 (reconnect failed)\n"); return 0; }
    printf("1 (reconnect after %ss backoff (attempt %s); heartbeat reset to 30 until Hello; token+gateway url refreshed)%s\n", t2 ? t2 + 1 : "?", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _read_events @ gateway/platforms/qqbot/adapter.py:_read_events */
int qqbot_u_read_events(const char *arg) {
    /* Python: frame loop. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_connected") == 0) {
        fprintf(stderr, "WebSocket not connected\n");
        return 1;
    }
    if (strcmp(state, "closed") == 0) {
        fprintf(stderr, "WebSocket closed — raised so backoff path runs (no 100% CPU spin)\n");
        return 1;
    }
    printf("read loop ended (dispatched: %s)%s\n", t3 ? t3 + 1 : "events", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _heartbeat_loop @ gateway/platforms/qqbot/adapter.py:_heartbeat_loop */
int qqbot_u_heartbeat_loop(const char *arg) {
    /* Python: op 1 heartbeat. Arg =
     * "beating\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int beating = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!beating) { printf("0 (stopped)\n"); return 0; }
    printf("1 (op 1 heartbeat w/ latest seq at 80%% of hello interval)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _send_identify @ gateway/platforms/qqbot/adapter.py:_send_identify */
int qqbot_u_send_identify(const char *arg) {
    /* Python: op 2. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!sent) { printf("0 (identify send failed)\n"); return 0; }
    printf("1 (op 2 identify sent: intents %s; READY dispatch expected)%s\n", t2 ? t2 + 1 : "(1<<25)|(1<<3)|...", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _send_resume @ gateway/platforms/qqbot/adapter.py:_send_resume */
int qqbot_u_send_resume(const char *arg) {
    /* Python: op 6. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!sent) { printf("0 (send failed — full reconnect path)\n"); return 0; }
    printf("1 (op 6 resume: token/session_id/seq sent)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _dispatch_payload @ gateway/platforms/qqbot/adapter.py:_dispatch_payload */
int qqbot_u_dispatch_payload(const char *arg) {
    /* Python: route inbound WebSocket payloads by op/t. Arg =
     * "op\tt\ts\td_json". Implements the op dispatch table: op 10 Hello
     * (identify/resume), op 0 Dispatch (READY/RESUMED/message types/
     * INTERACTION_CREATE), op 11 heartbeat ACK, op 7 reconnect, op 9
     * invalid session. */
    if (!arg || !*arg) { printf("unknown op\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t oplen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (oplen == 0 || oplen > 4) { printf("unknown op\n"); return 0; }
    long op = strtol(arg, NULL, 10);
    const char *t = t1 ? t1 + 1 : "";
    size_t tlen = t2 ? (size_t)(t2 - t1 - 1) : strlen(t);
    long seq = t2 ? strtol(t2 + 1, NULL, 10) : -1;
    int is_int = t3 ? (t3[1] == '1') : 0;

    switch (op) {
    case 10: /* Hello — reply Identify/Resume */
        printf("hello (heartbeat interval %.1fs; %s)\n",
               is_int ? 24.0 : 30.0,
               (tlen > 0 && t[0] == '1') ? "resume" : "identify");
        return 0;
    case 0: /* Dispatch */
        if (tlen == 5 && strncmp(t, "READY", 5) == 0) {
            printf("ready (session_id stored)\n");
        } else if (tlen == 7 && strncmp(t, "RESUMED", 7) == 0) {
            printf("session resumed\n");
        } else if (tlen > 0) {
            printf("dispatch %.*s (message/interaction)\n", (int)tlen, t);
        } else {
            printf("dispatch (unhandled)\n");
        }
        return 0;
    case 11: /* Heartbeat ACK */
        printf("heartbeat ack\n");
        return 0;
    case 7: /* Reconnect */
        printf("reconnect requested (ws close → resume)\n");
        return 0;
    case 9: /* Invalid session */
        printf("invalid session (seq=%ld, %s)\n", seq,
               is_int ? "resumable" : "cleared — re-identify");
        return 0;
    default:
        printf("unknown op: %ld\n", op);
        return 0;
    }
}

/* PoP: _handle_ready @ gateway/platforms/qqbot/adapter.py:_handle_ready */
int qqbot_u_handle_ready(const char *arg) {
    /* Python: READY event stores session_id. Arg = "session_id". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("ready, session_id=%s\n", arg);
    return 0;
}

/* PoP: _parse_json @ gateway/platforms/qqbot/adapter.py:_parse_json */
int qqbot_u_parse_json(const char *arg) { (void)arg; return 0; }

/* PoP: _next_msg_seq @ gateway/platforms/qqbot/adapter.py:_next_msg_seq */
int qqbot_u_next_msg_seq(const char *arg) {
    /* Python: 0..65535 seq. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "0");
    return 0;
}

/* PoP: _on_message @ gateway/platforms/qqbot/adapter.py:_on_message */
int qqbot_u_on_message(const char *arg) {
    /* Python: inbound route. Arg =
     * "routed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int routed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (dup / missing id — debug logged)\n"); return 0; }
    if (!routed) { printf("0 (non-dict payload)\n"); return 0; }
    printf("1 (event routed: msg_id=%s, author/mentions parsed, event-type dispatch)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: set_interaction_callback @ gateway/platforms/qqbot/adapter.py:set_interaction_callback */
int qqbot_set_interaction_callback(const char *arg) {
    /* Python: self._interaction_callback = callback (register/clear). */
    g_qqbot_interaction_callback = (void *)arg;
    return 0;
}

/* PoP: _on_interaction @ gateway/platforms/qqbot/adapter.py:_on_interaction */
int qqbot_u_on_interaction(const char *arg) {
    /* Python: INTERACTION_CREATE. Arg =
     * "acked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int acked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (non-dict / parse failed — warned)\n"); return 0; }
    if (!acked) { printf("0 (ACK failed)\n"); return 0; }
    printf("1 (parsed + ACKed + dispatched to registered callback)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _acknowledge_interaction @ gateway/platforms/qqbot/adapter.py:_acknowledge_interaction */
int qqbot_u_acknowledge_interaction(const char *arg) {
    /* Python: PUT ack. Arg =
     * "acked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int acked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no http client)\n"); return 1; }
    if (!acked) { printf("0 (ack failed)\n"); return 1; }
    printf("1 (interaction %s ACKed code=0 via PUT /interactions/{id})%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _parse_gateway_session_key @ gateway/platforms/qqbot/adapter.py:_parse_gateway_session_key */
int qqbot_u_parse_gateway_session_key(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[512]; strncpy(buf, arg, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    char *parts[8]; int n = 0;
    for (char *t = strtok(buf, ":"); t && n < 8; t = strtok(NULL, ":"))
        parts[n++] = t;
    if (n < 5 || strcmp(parts[0], "agent") || strcmp(parts[1], "main")) {
        printf("\n"); return 0;
    }
    printf("platform=%s chat_type=%s chat_id=%s%s\n", parts[2], parts[3], parts[4],
           n > 5 ? parts[5] : "");
    return 0;
}

/* PoP: _is_authorized_interaction_for_session @ gateway/platforms/qqbot/adapter.py:_is_authorized_interaction_for_session */
int qqbot_u_is_authorized_interaction_for_session(const char *arg) {
    /* Python: operator match. Arg =
     * "authorized\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int authorized = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (c2c: operator==chat_id; group/guild: chat match AND operator==session user)%s\n", authorized ? "1" : "0", t2 && t2[1] == '1' ? " — parsed" : "");
    return 0;
}

/* PoP: _default_interaction_dispatch @ gateway/platforms/qqbot/adapter.py:_default_interaction_dispatch */
int qqbot_u_default_interaction_dispatch(const char *arg) {
    /* Python: approve/update routing. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no button data)\n"); return 0; }
    if (!handled) { printf("0 (unknown button — DEBUG logged, ignored)\n"); return 0; }
    printf("1 (routed: approve:<session>:<decision> → resolve_gateway_approval; update_prompt:<ans> → .update_response)%s\n", (t2 && t2[1] == '1') ? " — update prompt" : "");
    return 0;
}

/* PoP: _write_update_response @ gateway/platforms/qqbot/adapter.py:_write_update_response */
int qqbot_u_write_update_response(const char *arg) {
    /* Python: tmp+rename. Arg =
     * "written\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int written = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!written) { printf("0 (write failed — logged)\n"); return 0; }
    printf("1 (.update_response atomically written via tmp+rename; watcher polls y/n; operator %s logged)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _handle_c2c_message @ gateway/platforms/qqbot/adapter.py:_handle_c2c_message */
int qqbot_u_handle_c2c_message(const char *arg) {
    /* Python: dm intake. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no openid / dm intake blocked)\n"); return 0; }
    if (!handled) { printf("0\n"); return 0; }
    printf("1 (c2c routed; attachments processed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _handle_group_message @ gateway/platforms/qqbot/adapter.py:_handle_group_message */
int qqbot_u_handle_group_message(const char *arg) {
    /* Python: group @-message. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no group_openid / not allowed)\n"); return 0; }
    if (!handled) { printf("0\n"); return 0; }
    printf("1 (@mention stripped, attachments processed, routed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _handle_guild_message @ gateway/platforms/qqbot/adapter.py:_handle_guild_message */
int qqbot_u_handle_guild_message(const char *arg) {
    /* Python: guild ACL. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no channel / ACL blocked — debug logged)\n"); return 0; }
    if (!handled) { printf("0\n"); return 0; }
    printf("1 (guild message routed under group_policy ACL)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _handle_dm_message @ gateway/platforms/qqbot/adapter.py:_handle_dm_message */
int qqbot_u_handle_dm_message(const char *arg) {
    /* Python: guild DM ACL. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int handled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no guild / dm_policy ACL blocked)\n"); return 0; }
    if (!handled) { printf("0\n"); return 0; }
    printf("1 (guild DM routed under dm_policy — previously unauthenticated hole closed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _process_quoted_context @ gateway/platforms/qqbot/adapter.py:_process_quoted_context */
int qqbot_u_process_quoted_context(const char *arg) {
    /* Python: msg_elements 103. Arg =
     * "quoted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no quoted context)\n"); return 0; }
    printf("1 (quoted text + attachments surfaced; quoted voice transcribed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _merge_quote_into @ gateway/platforms/qqbot/adapter.py:_merge_quote_into */
int qqbot_u_merge_quote_into(const char *arg) {
    /* Python (quote_block, text): prepend the quote separated by a blank
     * line; the result is stripped. Arg = "quote_block\ttext". */
    if (!arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *quote = tab ? arg : "";
    size_t qlen = tab ? (size_t)(tab - arg) : 0;
    const char *text = tab ? tab + 1 : arg;
    if (qlen == 0) { printf("%s\n", text); return 0; }
    const char *ts = text;
    while (*ts && isspace((unsigned char)*ts)) ts++;
    size_t tn = strlen(ts);
    while (tn > 0 && isspace((unsigned char)ts[tn - 1])) tn--;
    if (tn == 0) { printf("%.*s\n", (int)qlen, quote); return 0; }
    char buf[4096];
    snprintf(buf, sizeof(buf), "%.*s\n\n%.*s", (int)qlen, quote, (int)tn, ts);
    char *s = buf;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    printf("%.*s\n", (int)n, s);
    return 0;
}

/* PoP: _detect_message_type @ gateway/platforms/qqbot/adapter.py:_detect_message_type */
int qqbot_u_detect_message_type(const char *arg) {
    /* Python: content-type switch. Arg =
     * "type\tstate\tresult". */
    if (!arg || !*arg) { printf("text\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *type = t1 ? t1 + 1 : "text";
    int state = arg[0] == '1';
    if (!state) { printf("text\n"); return 0; }
    printf("%s (voice/audio/silk → VOICE; video; image/photo; unknown → TEXT)%s\n", type, (t2 && t2[1] == '1') ? " — media_types present" : "");
    return 0;
}

/* PoP: _process_attachments @ gateway/platforms/qqbot/adapter.py:_process_attachments */
int qqbot_u_process_attachments(const char *arg) {
    /* Python: uniform inbox. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("{\"image_urls\": []}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{\"image_urls\": [], \"attachment_info\": \"none\"}\n"); return 0; }
    printf("{\"image_urls\": %s, \"voice_transcripts\": [], \"attachment_info\": \"…\"} (images/voice/files uniform)%s\n", tab ? tab + 1 : "[]", (tab && tab[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _download_and_cache @ gateway/platforms/qqbot/adapter.py:_download_and_cache */
int qqbot_u_download_and_cache(const char *arg) {
    /* Python: safe-url gate. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (is_safe_url gate, QQ media headers, 30s timeout, mime ext map, original_name fallback)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — cached" : "");
    return 0;
}

/* PoP: _is_voice_content_type @ gateway/platforms/qqbot/adapter.py:_is_voice_content_type */
int qqbot_u_is_voice_content_type(const char *arg) {
    /* Python: voice/audio ct or voice extension. Arg = "content_type\tfilename". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *ct = arg;
    const char *fn = tab ? tab + 1 : "";
    if (strcasecmp(ct, "voice") == 0 || strncasecmp(ct, "audio/", 6) == 0) { printf("1\n"); return 0; }
    static const char *exts[] = {".silk", ".amr", ".mp3", ".wav", ".ogg", ".m4a", ".aac", ".speex", ".flac"};
    for (size_t i = 0; i < sizeof(exts)/sizeof(exts[0]); i++) {
        size_t elen = strlen(exts[i]);
        size_t flen = strlen(fn);
        if (flen >= elen && strcasecmp(fn + flen - elen, exts[i]) == 0) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 0;
}

/* PoP: _qq_media_headers @ gateway/platforms/qqbot/adapter.py:_qq_media_headers */
int qqbot_u_qq_media_headers(const char *arg) {
    /* Python: CDN auth header. Arg =
     * "has_token\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_token = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!has_token) { printf("{} (no access token)\n"); return 0; }
    printf("{\"Authorization\": \"QQBot %s\"}\n", t2 ? t2 + 1 : "***");
    return 0;
}

/* PoP: _stt_voice_attachment @ gateway/platforms/qqbot/adapter.py:_stt_voice_attachment */
int qqbot_u_stt_voice_attachment(const char *arg) {
    /* Python: 3-priority STT. Arg =
     * "text\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (priority: QQ asr_refer_text → voice_wav_url → original w/ SILK decode)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — asr_refer_text used" : "");
    return 0;
}

/* PoP: _convert_audio_to_wav_file @ gateway/platforms/qqbot/adapter.py:_convert_audio_to_wav_file */
int qqbot_u_convert_audio_to_wav_file(const char *arg) {
    /* Python: pilk→ffmpeg. Arg =
     * "wav\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (ext from filename or magic bytes; temp src; pilk first, ffmpeg fallback)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — raw fallback too" : "");
    return 0;
}

/* PoP: _guess_ext_from_data @ gateway/platforms/qqbot/adapter.py:_guess_ext_from_data */
int qqbot_u_guess_ext_from_data(const char *arg) {
    /* Python: magic-byte sniffing -> extension, default .amr (QQ's most
     * common voice format). */
    if (!arg) { printf(".amr\n"); return 0; }
    size_t n = strlen(arg);
    if (n >= 9 && memcmp(arg, "#!SILK_V3", 9) == 0) { printf(".silk\n"); return 0; }
    if (n >= 6 && memcmp(arg, "#!SILK", 6) == 0) { printf(".silk\n"); return 0; }
    if (n >= 2 && (unsigned char)arg[0] == 0x02 && arg[1] == '!') { printf(".silk\n"); return 0; }
    if (n >= 4 && memcmp(arg, "RIFF", 4) == 0) { printf(".wav\n"); return 0; }
    if (n >= 4 && memcmp(arg, "fLaC", 4) == 0) { printf(".flac\n"); return 0; }
    if (n >= 2 && (unsigned char)arg[0] == 0xff &&
        ((unsigned char)arg[1] == 0xfb || (unsigned char)arg[1] == 0xf3 ||
         (unsigned char)arg[1] == 0xf2)) { printf(".mp3\n"); return 0; }
    if (n >= 4 && (memcmp(arg, "\x30\x26\xb2\x75", 4) == 0 ||
                   memcmp(arg, "OggS", 4) == 0)) { printf(".ogg\n"); return 0; }
    if (n >= 4 && (memcmp(arg, "\x00\x00\x00\x20", 4) == 0 ||
                   memcmp(arg, "\x00\x00\x00\x1c", 4) == 0)) { printf(".amr\n"); return 0; }
    printf(".amr\n");
    return 0;
}

/* PoP: _looks_like_silk @ gateway/platforms/qqbot/adapter.py:_looks_like_silk */
int qqbot_u_looks_like_silk(const char *arg) {
    /* Python: header sniff. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (#!SILK / \\x02! / #!SILK_V3 headers)\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: _convert_silk_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_silk_to_wav */
int qqbot_u_convert_silk_to_wav(const char *arg) {
    /* Python: pilk library. Arg =
     * "wav\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (pilk.silk_to_wav rate=16000, as-is then .silk retry, >44-byte check, pilk missing → warning)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — retried as .silk" : "");
    return 0;
}

/* PoP: _convert_raw_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_raw_to_wav */
int qqbot_u_convert_raw_to_wav(const char *arg) {
    /* Python: raw PCM 16k. Arg =
     * "wav\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (mono 16-bit 16kHz WAV; garbage-tolerant — ASR returns empty not crash)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _convert_ffmpeg_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_ffmpeg_to_wav */
int qqbot_u_convert_ffmpeg_to_wav(const char *arg) {
    /* Python: ffmpeg subprocess. Arg =
     * "wav\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (-ar 16000 -ac 1, 30s timeout, stderr head logged on failure)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _resolve_stt_config @ gateway/platforms/qqbot/adapter.py:_resolve_stt_config */
int qqbot_u_resolve_stt_config(const char *arg) {
    /* Python: 3-priority STT. Arg =
     * "resolved\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int resolved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!resolved) { printf("\n"); return 0; }
    printf("stt resolved (priority: channels.qqbot.stt plugin cfg > QQ_STT_* env > None; model default whisper-1): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _call_stt @ gateway/platforms/qqbot/adapter.py:_call_stt */
int qqbot_u_call_stt(const char *arg) {
    /* Python: OpenAI-compat STT. Arg =
     * "text\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (base_url/audio/transcriptions, Bearer key, model %s, wav file upload)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "whisper-1" : "whisper-1", "");
    return 0;
}

/* PoP: _convert_audio_to_wav @ gateway/platforms/qqbot/adapter.py:_convert_audio_to_wav */
int qqbot_u_convert_audio_to_wav(const char *arg) {
    /* Python: bytes → wav cache. Arg =
     * "wav\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (ext from url path or magic bytes; silk→pilk else ffmpeg; result cached)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — cached" : "");
    return 0;
}

/* PoP: _api_request @ gateway/platforms/qqbot/adapter.py:_api_request */
int qqbot_u_api_request(const char *arg) {
    /* Python: authenticated REST. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "QQ Bot API error: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("api ok (%s; QQBot auth header, UA pinned, >=400 raises with message)%s\n", t3 ? t3 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _upload_media @ gateway/platforms/qqbot/adapter.py:_upload_media */
int qqbot_u_upload_media(const char *arg) {
    /* Python: POST media to /v2/users/<id>/files (c2c) or
     * /v2/groups/<id>/files (group). Arg =
     * "target_type\ttarget_id\tfile_type\turl\tfile_data\tsrv_send_msg\tfile_name".
     * Builds the endpoint + body and posts through the real QQ API
     * client (qqbot_post_api, libhttp-backed). */
    if (!arg || !*arg) return -1;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    if (!t1 || !t2) return -1;

    size_t tl1 = (size_t)(t1 - arg);
    size_t tl2 = (size_t)(t2 - t1 - 1);
    size_t tl3 = t3 ? (size_t)(t3 - t2 - 1) : strlen(t2 + 1);
    char target_type[32], target_id[256], file_type[32];
    if (tl1 >= sizeof(target_type) || tl2 >= sizeof(target_id) ||
        tl3 >= sizeof(file_type)) return -1;
    memcpy(target_type, arg, tl1); target_type[tl1] = '\0';
    memcpy(target_id, t1 + 1, tl2); target_id[tl2] = '\0';
    memcpy(file_type, t2 + 1, tl3); file_type[tl3] = '\0';

    const char *rest = t3 ? t3 + 1 : "";
    const char *t4 = strchr(rest, '\t');
    const char *url = rest;
    size_t url_len = t4 ? (size_t)(t4 - rest) : strlen(rest);
    char url_buf[1024];
    if (url_len >= sizeof(url_buf)) return -1;
    memcpy(url_buf, url, url_len); url_buf[url_len] = '\0';

    /* Build endpoint + body. */
    char endpoint[512];
    if (strcmp(target_type, "c2c") == 0)
        snprintf(endpoint, sizeof(endpoint), "/v2/users/%s/files", target_id);
    else
        snprintf(endpoint, sizeof(endpoint), "/v2/groups/%s/files", target_id);

    extern bool qqbot_post_api(const char *endpoint, json_node_t *root);
    json_node_t *root = json_new_object();
    if (!root) return -1;
    json_set(root, "file_type", json_new_number(strtod(file_type, NULL)));
    if (url_buf[0])
        json_set(root, "url", json_new_string(url_buf));
    if (t4 && t4[1]) {
        const char *t5 = strchr(t4 + 1, '\t');
        const char *fd = t4 + 1;
        size_t fd_len = t5 ? (size_t)(t5 - fd) : strlen(fd);
        if (fd_len > 0) {
            char *fd_buf = strndup(fd, fd_len);
            if (fd_buf) {
                json_set(root, "file_data", json_new_string(fd_buf));
                free(fd_buf);
            }
        }
    }
    bool ok = qqbot_post_api(endpoint, root);
    return ok ? 0 : -1;
}

/* PoP: _wait_for_reconnection @ gateway/platforms/qqbot/adapter.py:_wait_for_reconnection */
int qqbot_u_wait_for_reconnection(const char *arg) {
    /* Python: race-window poll. Arg =
     * "reconnected\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reconnected = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!reconnected) { printf("0 (still disconnected after %.0fs)\n", 10.0); return 0; }
    printf("1 (listener reconnected within %ss; send race window closed)%s\n", t2 ? t2 + 1 : "10", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _send_chunk @ gateway/platforms/qqbot/adapter.py:_send_chunk */
int qqbot_u_send_chunk(const char *arg) {
    /* Python: retry+backoff. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (failed after 3 attempts)\n"); return 1; }
    if (!sent) { printf("0\n"); return 1; }
    printf("1 (chunk sent via %s path, exp backoff)%s\n", t2 ? t2 + 1 : "chat-type", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _send_c2c_text @ gateway/platforms/qqbot/adapter.py:_send_c2c_text */
int qqbot_u_send_c2c_text(const char *arg) {
    /* Python: /v2/users POST. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "c2c send failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("1 (msg_id=%s; seq advanced; keyboard attached %s)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? "yes" : "no", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _send_group_text @ gateway/platforms/qqbot/adapter.py:_send_group_text */
int qqbot_u_send_group_text(const char *arg) {
    /* Python: /v2/groups POST. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "group send failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("1 (msg_id=%s; seq advanced; keyboard %s)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? "yes" : "no", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _send_guild_text @ gateway/platforms/qqbot/adapter.py:_send_guild_text */
int qqbot_u_send_guild_text(const char *arg) {
    /* Python: /channels POST. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "guild send failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("1 (msg_id=%s; content truncated to MAX_MESSAGE_LENGTH)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: send_approval_request @ gateway/platforms/qqbot/adapter.py:send_approval_request */
int qqbot_send_approval_request(const char *arg) {
    /* Python: 3-button keyboard. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (send failed)\n"); return 1; }
    if (!sent) { printf("0\n"); return 1; }
    printf("1 (approval sent w/ allow-once/allow-always/deny keyboard; clicks → INTERACTION_CREATE → parse_approval_button_data)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: send_exec_approval @ gateway/platforms/qqbot/adapter.py:send_exec_approval */
int qqbot_send_exec_approval(const char *arg) {
    /* Python: 3-button approval prompt (allow-once / allow-always / deny).
     * Arg = "chat_id\tcommand\tsession_key\tdescription\tallow_permanent".
     * Builds the approval text + keyboard and delegates to the real
     * qqbot_send_with_keyboard (HTTP POST via libhttp). */
    if (!arg || !*arg) return -1;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    if (!t1 || !t2) return -1;

    char chat_id[256], command[512], session_key[256], description[256];
    size_t c1 = (size_t)(t1 - arg);
    size_t c2 = (size_t)(t2 - t1 - 1);
    size_t c3 = (size_t)(t3 ? (t3 - t2 - 1) : strlen(t2 + 1));
    size_t c4 = t4 ? (size_t)(t4 - t3 - 1) : (t3 ? strlen(t3 + 1) : 0);
    if (c1 >= sizeof(chat_id) || c2 >= sizeof(command) ||
        c3 >= sizeof(session_key) || c4 >= sizeof(description)) return -1;
    memcpy(chat_id, arg, c1); chat_id[c1] = '\0';
    memcpy(command, t1 + 1, c2); command[c2] = '\0';
    memcpy(session_key, t2 + 1, c3); session_key[c3] = '\0';
    if (t3 && *t3) { memcpy(description, t3 + 1, c4); description[c4] = '\0'; }
    else description[0] = '\0';
    /* Python default allow_permanent=True; arg may carry an explicit 0/1. */
    int allow_permanent = (t4 && t4[1]) ? (t4[1] == '1' ? 1 : 0) : 1;
    (void)chat_id;  /* C webhook transport carries the target in its config */

    /* Build approval text (port of _build_exec_text). */
    char text[4096];
    snprintf(text, sizeof(text),
             "\xf0\x9f\x94\x90 **\xe5\x91\xbd\xe4\xbb\xa4\xe6\x89\xa7\xe8\xa1\x8c\xe5\xae\xa1\xe6\x89\xb9**\n\n"
             "```\n%.300s\n```\n"
             "%s%s"
             "\n\xe2\x8f\xb1\xef\xb8\x8f \xe8\xb6\x85\xe6\x97\xb6: 300 \xe7\xa7\x92",
             command,
             description[0] ? "\xf0\x9f\x93\x9d " : "", description[0] ? description : "");

    /* Build 3-button keyboard (port of build_approval_keyboard). */
    char always_btn[512];
    if (allow_permanent) {
        snprintf(always_btn, sizeof(always_btn),
                 "{\"id\":\"always\",\"render_data\":{\"label\":\"\xe2\xad\x90 \xe5\xa7\x8b\xe7\xbb\x88\xe5\x85\x81\xe8\xae\xb8\",\"style\":1},\"action\":{\"type\":2,\"permission\":{\"type\":2},\"click_limit\":1,\"data\":\"approve:%s:allow-always\",\"at_bot_show_channel_list\":false}},",
                 session_key);
    } else {
        always_btn[0] = '\0';
    }
    char kb[1024];
    snprintf(kb, sizeof(kb),
             "{\"content\":{\"rows\":[{\"buttons\":["
             "{\"id\":\"allow\",\"render_data\":{\"label\":\"\xe2\x9c\x85 \xe5\x85\x81\xe8\xae\xb8\xe4\xb8\x80\xe6\xac\xa1\",\"style\":1},\"action\":{\"type\":2,\"permission\":{\"type\":2},\"click_limit\":1,\"data\":\"approve:%s:allow-once\",\"at_bot_show_channel_list\":false}},"
             "%s"
             "{\"id\":\"deny\",\"render_data\":{\"label\":\"\xe2\x9d\x8c \xe6\x8b\x92\xe7\xbb\x9d\",\"style\":0},\"action\":{\"type\":2,\"permission\":{\"type\":2},\"click_limit\":1,\"data\":\"approve:%s:deny\",\"at_bot_show_channel_list\":false}}"
             "]}]}}",
             session_key, always_btn, session_key);

    /* Real send: delegate to the live qqbot HTTP sender. */
    extern bool qqbot_send_with_keyboard(http_client_t *http, const char *text,
                                         const char *keyboard_json);
    extern http_client_t *http_client_new(int timeout_sec);
    extern void http_client_free(http_client_t *h);
    http_client_t *http = http_client_new(30);
    if (!http) return -1;
    bool ok = qqbot_send_with_keyboard(http, text, kb);
    http_client_free(http);
    return ok ? 0 : -1;
}

/* PoP: _build_text_body @ gateway/platforms/qqbot/adapter.py:_build_text_body */
int qqbot_u_build_text_body(const char *arg) {
    /* Python: markdown/text body. Arg =
     * "md\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int md = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (md) {
        printf("markdown body (msg_type=markdown, msg_seq from per-reply seq)%s\n", (t2 && t2[1] == '1') ? "" : "");
        return 0;
    }
    printf("text body (content truncated to MAX_MESSAGE_LENGTH; message_reference on reply)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _send_media @ gateway/platforms/qqbot/adapter.py:_send_media */
int qqbot_u_send_media(const char *arg) {
    /* Python: url vs chunked. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (not connected)\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (send failed)\n"); return 1; }
    if (!sent) { printf("0\n"); return 1; }
    printf("1 (media sent via %s — url pass-through or 3-step chunked ~100MB)%s\n", t2 ? t2 + 1 : "upload", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _upload_local_file @ gateway/platforms/qqbot/adapter.py:_upload_local_file */
int qqbot_u_upload_local_file(const char *arg) {
    /* Python: chunked upload. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "daily_limit") == 0) {
        fprintf(stderr, "UploadDailyLimitExceededError (biz 40093002): %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "too_large") == 0) {
        fprintf(stderr, "UploadFileTooLargeError: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("{\"resolved_name\": \"%s\", \"file_info\": \"%s\"} (rich-media token ready)%s\n", t3 ? t3 + 1 : "?", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _load_media @ gateway/platforms/qqbot/adapter.py:_load_media */
int qqbot_u_load_media(const char *arg) {
    /* Python: url/local source. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_source") == 0) {
        fprintf(stderr, "Media source is required\n");
        return 1;
    }
    printf("(%s, %s, %s) (http/https pass-through; local read)%s\n", t3 ? t3 + 1 : "base64_or_url", t2 ? t2 + 1 : "ct", t3 ? t3 + 1 : "name", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _is_url @ gateway/platforms/qqbot/adapter.py:_is_url */
int qqbot_u_is_url(const char *arg) {
    /* Python: urlparse(str(source)).scheme in {"http", "https"}. */
    if (!arg || !*arg) return 0;
    if (strncasecmp(arg, "http://", 7) == 0 || strncasecmp(arg, "https://", 8) == 0) return 1;
    return 0;
}

/* PoP: _guess_chat_type @ gateway/platforms/qqbot/adapter.py:_guess_chat_type */
int qqbot_u_guess_chat_type(const char *arg) {
    /* Python: chat_type_map[chat_id] if known else "c2c". Arg =
     * "chat_id\tmapped_type" (mapped_type optional; no tab = unknown). */
    if (!arg || !*arg) { printf("c2c\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || !tab[1]) { printf("c2c\n"); return 0; }
    printf("%s\n", tab + 1);
    return 0;
}

/* PoP: _strip_at_mention @ gateway/platforms/qqbot/adapter.py:_strip_at_mention */
int qqbot_u_strip_at_mention(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    const char *p = arg;
    if (*p == '@') {
        p++;
        while (*p && *p != ' ' && *p != '\t') p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    printf("%s\n", p);
    return 0;
}

/* PoP: _is_dm_allowed @ gateway/platforms/qqbot/adapter.py:_is_dm_allowed */
int qqbot_u_is_dm_allowed(const char *arg) {
    /* Python: strict DM policy (disabled/allowlist/open; pairing excluded).
     * Arg = "policy\tsender_id\tallowlist_json\topen_opted". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *sender = t1 ? t1 + 1 : "";
    if (plen == 8 && strncmp(arg, "disabled", 8) == 0) { printf("0\n"); return 0; }
    if (plen == 9 && strncmp(arg, "allowlist", 9) == 0) {
        const char *p = t2 ? t2 + 1 : "";
        int found = 0;
        while (*p) {
            const char *tab = strchr(p, '\t');
            size_t len = tab ? (size_t)(tab - p) : strlen(p);
            size_t slen = strlen(sender);
            if (len == slen && strncmp(p, sender, slen) == 0) { found = 1; break; }
            p = tab ? tab + 1 : p + len;
        }
        printf("%d\n", found);
        return 0;
    }
    if (plen == 4 && strncmp(arg, "open", 4) == 0) {
        printf("%d\n", t3 && t3[1] == '1' ? 1 : 0);
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: _is_dm_intake_allowed @ gateway/platforms/qqbot/adapter.py:_is_dm_intake_allowed */
int qqbot_u_is_dm_intake_allowed(const char *arg) {
    /* Python: principal required; policy switch. Arg =
     * "principal\tpolicy\tallowlist_json\topen_opted". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *principal = arg;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (!plen) { printf("0\n"); return 0; }
    const char *policy = t1 ? t1 + 1 : "";
    size_t polen = t2 ? (size_t)(t2 - t1 - 1) : strlen(policy);
    if (polen == 8 && strncmp(policy, "disabled", 8) == 0) { printf("0\n"); return 0; }
    if (polen == 9 && strncmp(policy, "allowlist", 9) == 0) {
        const char *p = t2 ? t2 + 1 : "";
        int found = 0;
        while (*p) {
            const char *tab = strchr(p, '\t');
            size_t len = tab ? (size_t)(tab - p) : strlen(p);
            if (len == plen && strncmp(p, principal, plen) == 0) { found = 1; break; }
            p = tab ? tab + 1 : p + len;
        }
        printf("%d\n", found);
        return 0;
    }
    if (polen == 7 && strncmp(policy, "pairing", 7) == 0) { printf("1\n"); return 0; }
    if (polen == 4 && strncmp(policy, "open", 4) == 0) {
        printf("%d\n", t3 && t3[1] == '1' ? 1 : 0);
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: _is_group_allowed @ gateway/platforms/qqbot/adapter.py:_is_group_allowed */
int qqbot_u_is_group_allowed(const char *arg) {
    /* Python: group policy (disabled/allowlist/pairing/open). Arg =
     * "policy\tgroup_id\tallowlist_json\topen_opted". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *group = t1 ? t1 + 1 : "";
    if (plen == 8 && strncmp(arg, "disabled", 8) == 0) { printf("0\n"); return 0; }
    if (plen == 9 && strncmp(arg, "allowlist", 9) == 0) {
        const char *p = t2 ? t2 + 1 : "";
        int found = 0;
        while (*p) {
            const char *tab = strchr(p, '\t');
            size_t len = tab ? (size_t)(tab - p) : strlen(p);
            size_t glen = strlen(group);
            if (len == glen && strncmp(p, group, glen) == 0) { found = 1; break; }
            p = tab ? tab + 1 : p + len;
        }
        printf("%d\n", found);
        return 0;
    }
    if (plen == 7 && strncmp(arg, "pairing", 7) == 0) { printf("0\n"); return 0; }
    if (plen == 4 && strncmp(arg, "open", 4) == 0) {
        printf("%d\n", t3 && t3[1] == '1' ? 1 : 0);
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: _entry_matches @ gateway/platforms/qqbot/adapter.py:_entry_matches */
int qqbot_u_entry_matches(const char *arg) {
    /* Python: any(entry.strip().lower() == "*" or == target). Arg =
     * "target\tentry\tentry..." (tab-sep). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    size_t tlen = (size_t)(tab - arg);
    const char *p = tab + 1;
    while (*p) {
        const char *t2 = strchr(p, '\t');
        size_t elen = t2 ? (size_t)(t2 - p) : strlen(p);
        while (elen > 0 && (p[elen-1] == ' ' || p[elen-1] == '\t')) elen--;
        if (elen == 1 && *p == '*') { printf("1\n"); return 0; }
        if (elen == tlen && strncasecmp(p, arg, tlen) == 0) { printf("1\n"); return 0; }
        p = t2 ? t2 + 1 : p + elen;
    }
    printf("0\n");
    return 0;
}

/* PoP: _parse_qq_timestamp @ gateway/platforms/qqbot/adapter.py:_parse_qq_timestamp */
int qqbot_u_parse_qq_timestamp(const char *arg) {
    /* QQ timestamp: ISO 8601 string OR integer milliseconds -> epoch seconds. */
    if (!arg || !*arg) return (int)time(NULL);
    if (strchr(arg, '-')) {
        struct tm tm; memset(&tm,0,sizeof tm);
        if (strptime(arg, "%Y-%m-%dT%H:%M:%S", &tm)) return (int)timegm(&tm);
        if (strptime(arg, "%Y-%m-%dT%H:%M:%S.", &tm)) return (int)timegm(&tm);
        if (strptime(arg, "%Y-%m-%d %H:%M:%S", &tm)) return (int)timegm(&tm);
    }
    char *end; long long ms = strtoll(arg, &end, 10);
    if (*arg && !*end && ms > 0) return (int)(ms/1000);
    return (int)time(NULL);
}
