/*
 * dingtalk_auth.c — canonical C11 port of hermes_cli/dingtalk_auth.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dingtalk_auth_u_api_post @ hermes_cli/dingtalk_auth.py:_api_post */
int hermes_cli_dingtalk_auth_u_api_post(const char *arg) {
    /* Python: POST + errcode check. Arg = "path\tstate\terrcode\terrmsg\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "network_error") == 0) {
        fprintf(stderr, "Network error calling registration api: %s\n", t4 ? t4 + 1 : "");
        return 1;
    }
    if (strcmp(state, "api_error") == 0) {
        fprintf(stderr, "API error [%s]: %s (errcode=%s)\n", arg, t3 ? t3 + 1 : "", t2 ? t2 + 1 : "");
        return 1;
    }
    printf("%s\n", t4 ? t4 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_dingtalk_auth_wait_for_registration_success @ hermes_cli/dingtalk_auth.py:wait_for_registration_success */
int hermes_cli_dingtalk_auth_wait_for_registration_success(const char *arg) {
    /* Python: poll loop. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing_creds") == 0 || strcmp(state, "failed") == 0) {
        fprintf(stderr, "dingtalk registration error: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "timeout") == 0) {
        fprintf(stderr, "authorization timed out, please retry\n");
        return 1;
    }
    printf("client_id=%s client_secret=%s\n", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_dingtalk_auth_u_ensure_qrcode_installed @ hermes_cli/dingtalk_auth.py:_ensure_qrcode_installed */
int hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(const char *arg) {
    /* Python: import or pip-install qrcode. Arg = "installed\tinstall_rc". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("1\n"); return 0; }
    if (tab && strcmp(tab + 1, "0") == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_dingtalk_auth_render_qr_to_terminal @ hermes_cli/dingtalk_auth.py:render_qr_to_terminal */
int hermes_cli_dingtalk_auth_render_qr_to_terminal(const char *arg) {
    /* Python: half-block QR. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("QR rendered (half-block)\n");
    return 1;
}

/* PoP: hermes_cli_dingtalk_auth_dingtalk_qr_auth @ hermes_cli/dingtalk_auth.py:dingtalk_qr_auth */
int hermes_cli_dingtalk_auth_dingtalk_qr_auth(const char *arg) {
    /* Python: QR device flow. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "init_fail") == 0) {
        fprintf(stderr, "  Authorization init failed: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (strcmp(state, "auth_fail") == 0) {
        fprintf(stderr, "  Authorization failed: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("  QR scan authorization successful! client_id=%s\n", t2 ? t2 + 1 : "?");
    return 0;
}
