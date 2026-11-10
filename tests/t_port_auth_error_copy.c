/* t_port_auth_error_copy.c — Oracle harness.
 * Emits the FULL afc_error_t input + host/retry for each case so the oracle can
 * reconstruct the matching Python exception and replay the live functions.
 * Line shape: {"case","kind","mess":<json|null>,"type":<json|null>,
 *   "oauth":<json|null>,"code":<json|null>,"exit":int,
 *   "host":<json|null>,"retry":<json|null>,
 *   "signin":<json array|null>,"provider":<json array|null>}
 */
#include "port_auth_error_copy.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

static const char *afc_kind_name(afc_kind_t k) {
    switch (k) {
    case AFC_OTHER: return "other";
    case AFC_CANCELLED: return "cancelled";
    case AFC_NETWORK: return "network";
    case AFC_SIGNIN_COPY: return "signin_copy";
    case AFC_DEVICE_EXPIRED: return "device_expired";
    }
    return "other";
}

static void jstr(const char *s) {
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static void emit_arr(const char *key, json_t *arr) {
    printf(",\"%s\":", key);
    if (!arr) fputs("null", stdout);
    else { char *s = json_serialize(arr); fputs(s ? s : "null", stdout); free(s); json_free(arr); }
}

static void emit(const char *name, const afc_error_t *e,
                 const char *host, const char *retry) {
    printf("{\"case\":\"%s\",\"kind\":\"%s\",\"mess\":", name, afc_kind_name(e->kind));
    jstr(e->message);
    printf(",\"type\":");
    jstr(e->type_name);
    printf(",\"oauth\":");
    jstr(e->oauth_code);
    printf(",\"code\":");
    jstr(e->exc_code);
    printf(",\"exit\":%d,\"host\":", e->exit_code);
    jstr(host);
    printf(",\"retry\":");
    jstr(retry);
    emit_arr("signin", afc_sign_in_failure_lines(e, host ? host : "portal.nousresearch.com",
                                                 retry ? retry : "hermes portal"));
    emit_arr("provider", afc_provider_setup_failure_lines(e, retry ? retry : "hermes model"));
    printf("}\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *host = "portal.nousresearch.com", *retry = "hermes portal";

    afc_error_t cancelled  = { AFC_CANCELLED, "operation aborted", "KeyboardInterrupt", NULL, NULL, 0 };
    afc_error_t eof        = { AFC_CANCELLED, "EOF received", "EOFError", NULL, NULL, 0 };
    afc_error_t sysexit130 = { AFC_CANCELLED, "user interrupt", "SystemExit", NULL, NULL, 130 };
    afc_error_t sysexit_n  = { AFC_CANCELLED, "exit", "SystemExit", NULL, NULL, -1 };
    afc_error_t sysexit3   = { AFC_OTHER, "exit", "SystemExit", NULL, NULL, 3 };
    afc_error_t net        = { AFC_NETWORK, "connection refused", "ConnectError", NULL, NULL, 0 };
    afc_error_t net_connerr= { AFC_OTHER, "reset by peer", "ConnectionResetError", NULL, NULL, 0 };
    afc_error_t net_to     = { AFC_OTHER, "timed out", "ReadTimeout", NULL, NULL, 0 };
    afc_error_t generic    = { AFC_OTHER, "boom", "RuntimeError", NULL, NULL, 0 };
    afc_error_t generic_empty = { AFC_OTHER, "", "RuntimeError", NULL, NULL, 0 };
    afc_error_t copy       = { AFC_SIGNIN_COPY,
        "The sign-in code expired.\n  Details: expired_token: gone",
        "SignInCopyError", "expired_token", NULL, 0 };
    afc_error_t copy_single = { AFC_SIGNIN_COPY, "Sign-in did not complete.",
                                "SignInCopyError", "", NULL, 0 };
    afc_error_t dev_exp    = { AFC_DEVICE_EXPIRED, "device code timed out",
                               "DeviceCodeExpired", NULL, "device_code_timeout", 0 };
    afc_error_t dev_exp2   = { AFC_OTHER, "timed out", "AuthError", NULL, "timeout", 0 };
    afc_error_t net_custom = { AFC_NETWORK, "unreachable", "ConnectionError", NULL, NULL, 0 };

    emit("cancelled", &cancelled, host, retry);
    emit("eof", &eof, host, retry);
    emit("sysexit130", &sysexit130, host, retry);
    emit("sysexit_none", &sysexit_n, host, retry);
    emit("sysexit3", &sysexit3, host, retry);
    emit("network", &net, host, retry);
    emit("network_connerr", &net_connerr, host, retry);
    emit("network_timeout", &net_to, host, retry);
    emit("generic", &generic, host, retry);
    emit("generic_empty", &generic_empty, host, retry);
    emit("signin_copy", &copy, host, retry);
    emit("signin_copy_single", &copy_single, host, retry);
    emit("device_expired", &dev_exp, host, retry);
    emit("device_expired2", &dev_exp2, host, retry);
    emit("network_custom", &net_custom, "example.test", "hermes login");

    /* device-flow error text only (no exception) */
    static const char *const flows[][2] = {
        { "expired_token", "code expired" },
        { "access_denied", "user said no" },
        { "invalid_grant", "bad grant" },
        { "invalid_client", "unknown app" },
        { "weird_code", "server exploded" },
        { "", "no code given" },
    };
    for (size_t i = 0; i < sizeof(flows) / sizeof(flows[0]); i++) {
        printf("{\"case\":\"flow_%zu\",\"flow\":", i);
        jstr(flows[i][0]);
        printf(",\"desc\":");
        jstr(flows[i][1]);
        printf(",\"out\":");
        char *m = afc_device_flow_error(flows[i][0], flows[i][1], "hermes portal");
        jstr(m);
        printf("}\n");
        free(m);
    }
    return 0;
}
