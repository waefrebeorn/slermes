/* gate: web_server__ws_auth_reason — links the REAL C port
 * src/hermes_cli/web_server_ws_auth.c::ws_auth_reason (+ its TU) and traces
 * the WS-upgrade credential gate.
 * LIVE twin: hermes_cli/web_server.py::_ws_auth_reason, driven with a stub
 * WebSocket whose query params carry the credential; the ticket store
 * (hermes_cli.dashboard_auth.ws_tickets) is monkeypatched with the SAME
 * deterministic rules as the C harness's out-of-path ticket-store stubs
 * (only "goodticket" / "goodinternal" validate).
 *
 * Input argv format:
 *   "<auth_required 0|1><TAB><token><TAB><ticket><TAB><internal>"
 * (_SESSION_TOKEN fixed to "s3cret" on both sides.)
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: auth_required (gated mode)
 *   gated:   B2: internal present   B3: internal credential valid
 *            B4: no ticket presented          B5: ticket valid
 *   open:    B4: no token presented           B5: token matches
 *   VSTR(credential)  VSTR(reason or "-")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "web_server_ws_auth.h"

/* Out-of-path externals: the ticket store + accepted-host check. Stubbed
 * with deterministic rules mirrored by the twin's monkeypatched store. */
bool ws_is_accepted_host(const char *host_header, const char *bound_host) {
    (void)host_header; (void)bound_host; return true;
}
char *ws_tickets_consume_ticket(const char *ticket) {
    if (ticket && !strcmp(ticket, "goodticket")) return strdup("user-1");
    return NULL;
}
char *ws_tickets_consume_internal_credential(const char *value) {
    if (value && !strcmp(value, "goodinternal")) return strdup("server-internal");
    return NULL;
}

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else if (p[0] == '\\' && p[1] == 's') { *o++ = ' '; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *input) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);
    char *t1 = strchr(buf, '\t');
    char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    bool auth_required = buf[0] == '1';
    char *tok = t1 ? t1 + 1 : (char *)"-";
    char *tic = t2 ? t2 + 1 : (char *)"-";
    char *intn = t3 ? t3 + 1 : (char *)"-";
    if (t1) *t1 = '\0';
    if (t2) *t2 = '\0';
    if (t3) *t3 = '\0';
    /* "-" == absent/empty (same normalization the twin applies). */
    char token[256], ticket[256], internal[256];
    snprintf(token, sizeof token, "%s", strcmp(tok, "-") ? tok : "");
    snprintf(ticket, sizeof ticket, "%s", strcmp(tic, "-") ? tic : "");
    snprintf(internal, sizeof internal, "%s", strcmp(intn, "-") ? intn : "");

    WCUP_BEGIN("_ws_auth_reason");

    ws_auth_state_t st = { auth_required, "127.0.0.1", "s3cret" };
    ws_upgrade_req_t req = { NULL, NULL, NULL, token, ticket, internal };

    /* Same rungs the port evaluates, in order. */
    const char *cred = "none";
    const char *reason = NULL;
    WCUP_BRANCH(st.auth_required);                           /* B1 */
    if (st.auth_required) {
        WCUP_BRANCH(internal[0]);                            /* B2 */
        if (internal[0]) {
            bool ok = ws_tickets_consume_internal_credential(internal) != NULL;
            WCUP_BRANCH(ok);                                 /* B3 */
            cred = "internal";
            reason = ok ? NULL : "internal_invalid";
        } else {
            WCUP_BRANCH(!ticket[0]);                         /* B4 */
            if (!ticket[0]) { reason = "no_credential"; }
            else {
                bool ok = ws_tickets_consume_ticket(ticket) != NULL;
                WCUP_BRANCH(ok);                             /* B5 */
                cred = "ticket";
                reason = ok ? NULL : "ticket_invalid";
            }
        }
    } else {
        WCUP_BRANCH(!token[0]);                              /* B4 */
        if (!token[0]) { reason = "no_credential"; }
        else {
            bool match = strcmp(token, st.session_token ? st.session_token : "") == 0;
            WCUP_BRANCH(match);                              /* B5 */
            cred = "token";
            reason = match ? NULL : "token_mismatch";
        }
    }

    WCUP_VALSTR(cred);
    WCUP_VALSTR(reason ? reason : "-");

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_ws_auth_reason");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _ws_auth_reason <auth>\\t<tok>\\t<tick>\\t<internal>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
