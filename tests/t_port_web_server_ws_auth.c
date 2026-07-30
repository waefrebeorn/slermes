/*
 * t_port_web_server_ws_auth.c — oracle harness for the WS upgrade gate
 * family. Reads one fixture JSON, prints decision JSON.
 * Ops:
 *   {"op":"gates","state":{...},"req":{...}}   → all gate outputs at once
 *   {"op":"auth","state":{...},"req":{...},"mint":bool,"mint_expired":bool,
 *    "internal":bool,"double_consume":bool}    → _ws_auth_reason/_ok
 *   {"op":"query_token","state":{...},"path":"...","token":"..."}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_ws_auth.h"
#include "ws_tickets.h"

static void pjs(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p < 0x20) printf("\\u%04x", *p);
        else putchar(*p);
    }
}

static void emit_opt(const char *key, char *val, bool comma) {
    printf("\"%s\":", key);
    if (val) { putchar('"'); pjs(val); putchar('"'); free(val); }
    else fputs("null", stdout);
    if (comma) putchar(',');
}

static const char *gs(json_t *o, const char *k) {
    json_t *v = o ? json_object_get(o, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");

    json_t *st_j = json_object_get(fx, "state");
    json_t *rq_j = json_object_get(fx, "req");
    ws_auth_state_t st = {
        .auth_required = json_get_bool(st_j, "auth_required", false),
        .bound_host = gs(st_j, "bound_host"),
        .session_token = gs(st_j, "session_token"),
    };
    if (!st.session_token) st.session_token = "";
    ws_upgrade_req_t rq = {
        .client_host = gs(rq_j, "client_host"),
        .host_header = gs(rq_j, "host_header"),
        .origin_header = gs(rq_j, "origin_header"),
        .q_token = gs(rq_j, "token"),
        .q_ticket = gs(rq_j, "ticket"),
        .q_internal = gs(rq_j, "internal"),
    };

    if (strcmp(op, "gates") == 0) {
        putchar('{');
        emit_opt("client_reason", ws_auth_client_reason(&st, &rq), true);
        printf("\"client_allowed\":%s,",
               ws_auth_client_is_allowed(&st, &rq) ? "true" : "false");
        emit_opt("host_origin_reason", ws_auth_host_origin_reason(&st, &rq), true);
        printf("\"host_origin_allowed\":%s,",
               ws_auth_host_origin_is_allowed(&st, &rq) ? "true" : "false");
        emit_opt("request_reason", ws_auth_request_reason(&st, &rq), true);
        printf("\"request_allowed\":%s,",
               ws_auth_request_is_allowed(&st, &rq) ? "true" : "false");
        printf("\"mode\":\"%s\"}\n", ws_auth_mode(&st));
    } else if (strcmp(op, "auth") == 0) {
        /* Optional ticket-store setup driven by fixture flags. */
        char minted[256] = "";
        if (json_get_bool(fx, "mint", false)) {
            char *t = ws_tickets_mint_ticket("user-1", "github");
            snprintf(minted, sizeof(minted), "%s", t);
            free(t);
            rq.q_ticket = minted;
        }
        char internal_cred[256] = "";
        if (json_get_bool(fx, "internal", false)) {
            char *c = ws_tickets_internal_ws_credential();
            snprintf(internal_cred, sizeof(internal_cred), "%s", c);
            free(c);
            rq.q_internal = internal_cred;
        }
        if (json_get_bool(fx, "double_consume", false) && minted[0]) {
            char *first = ws_tickets_consume_ticket(minted);
            free(first); /* second consume inside ws_auth_reason must fail */
        }
        const char *cred = NULL;
        const char *reason = ws_auth_reason(&st, &rq, &cred);
        printf("{\"reason\":%s%s%s,\"credential\":\"%s\",\"ok\":%s}\n",
               reason ? "\"" : "", reason ? reason : "null", reason ? "\"" : "",
               cred, reason == NULL ? "true" : "false");
    } else if (strcmp(op, "query_token") == 0) {
        printf("{\"valid\":%s}\n",
               ws_auth_has_valid_query_token(&st, json_get_str(fx, "path", ""),
                                             json_get_str(fx, "token", NULL))
                   ? "true" : "false");
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
