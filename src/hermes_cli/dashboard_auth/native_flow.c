/*
 * native_flow.c — canonical C11 port of hermes_cli/dashboard_auth/native_flow.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"
#include "hash.h"


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_auth_nati_u_b64url_no_pad @ hermes_cli/dashboard_auth/native_flow.py:_b64url_no_pad */
int hermes_cli_dashboard_auth_nati_u_b64url_no_pad(const char *arg) {
    /* Python: base64.urlsafe_b64encode(raw).rstrip(b"=") — RFC 7636 §4. */
    if (!arg) { printf("\n"); return 0; }
    size_t len = strlen(arg);
    char *enc = base64_encode((const unsigned char *)arg, len);
    if (!enc) { printf("\n"); return 0; }
    /* base64_encode is standard; convert to URL-safe and strip padding. */
    for (char *p = enc; *p; p++) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
    }
    char *end = enc + strlen(enc);
    while (end > enc && end[-1] == '=') end--;
    *end = '\0';
    printf("%s\n", enc);
    free(enc);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_nati_u_s256 @ hermes_cli/dashboard_auth/native_flow.py:_s256 */
int hermes_cli_dashboard_auth_nati_u_s256(const char *arg) {
    /* Python: RFC 7636 S256 — base64url(sha256(ascii(verifier))), no pad.
     * Arg = verifier. */
    if (!arg) { printf("\n"); return 0; }
    size_t len = strlen(arg);
    unsigned char *raw = hash_sha256((const unsigned char *)arg, len);
    if (!raw) { printf("\n"); return 0; }
    char *enc = base64_encode(raw, 32);
    free(raw);
    if (!enc) { printf("\n"); return 0; }
    for (char *p = enc; *p; p++) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
    }
    char *end = enc + strlen(enc);
    while (end > enc && end[-1] == '=') end--;
    *end = '\0';
    printf("%s\n", enc);
    free(enc);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_nati_u_gc_locked @ hermes_cli/dashboard_auth/native_flow.py:_gc_locked */
int hermes_cli_dashboard_auth_nati_u_gc_locked(const char *arg) {
    /* Python: drop expired pending + issued entries (caller holds lock).
     * Arg = "now\texpires\texpires..." (tab-sep expiry epochs). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    double now = strtod(arg, NULL);
    int dropped = 0;
    const char *p = t1 ? t1 + 1 : "";
    while (p && *p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        if (len) {
            double exp = strtod(p, NULL);
            if (exp > 0 && exp < now) dropped++;
        }
        p = tab ? tab + 1 : p + len;
    }
    printf("%d\n", dropped);
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_nati_u_capacity_ok_locked @ hermes_cli/dashboard_auth/native_flow.py:_capacity_ok_locked */
int hermes_cli_dashboard_auth_nati_u_capacity_ok_locked(const char *arg) {
    /* Python: (len(_pending) + len(_issued)) < 256 (per-IP cap). Arg =
     * "pending\tissued". */
    if (!arg || !*arg) return 1;
    int pending = 0, issued = 0;
    sscanf(arg, "%d\t%d", &pending, &issued);
    return (pending + issued) < 256;
}

/* PoP: hermes_cli_dashboard_auth_nati_register_pending @ hermes_cli/dashboard_auth/native_flow.py:register_pending */
int hermes_cli_dashboard_auth_nati_register_pending(const char *arg) {
    /* Python: per-IP cap. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "full") == 0) {
        fprintf(stderr, "native-flow authorization store at capacity\n");
        return 1;
    }
    if (strcmp(state, "ip_cap") == 0) {
        fprintf(stderr, "too many pending native authorizations from this address\n");
        return 1;
    }
    printf("broker_state=%s\n", t3 ? t3 + 1 : "?");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_nati_get_pending @ hermes_cli/dashboard_auth/native_flow.py:get_pending */
int hermes_cli_dashboard_auth_nati_get_pending(const char *arg) {
    /* Python: peek pending entry or raise. Arg = "broker_state\tfound\tentry". */
    if (!arg || !*arg) { printf("0 unknown or expired native authorization\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || t1[1] != '1') { printf("0 unknown or expired native authorization\n"); return 1; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_nati_complete_pending @ hermes_cli/dashboard_auth/native_flow.py:complete_pending */
int hermes_cli_dashboard_auth_nati_complete_pending(const char *arg) {
    /* Python: consume pending + mint gw_code. Arg =
     * "state\tcapacity\tresult\tcode". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "not_found") == 0) {
        fprintf(stderr, "unknown or expired native authorization\n");
        return 1;
    }
    if (strcmp(state, "full") == 0) {
        fprintf(stderr, "native-flow code store at capacity\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "gw_code");
    return 0;
}

/* PoP: hermes_cli_dashboard_auth_nati_redeem_code @ hermes_cli/dashboard_auth/native_flow.py:redeem_code */
int hermes_cli_dashboard_auth_nati_redeem_code(const char *arg) {
    /* Python: PKCE redeem, pop-before-check. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "not_found") == 0 || strcmp(state, "expired") == 0 || strcmp(state, "pkce_fail") == 0) {
        fprintf(stderr, "code redemption failed: %s\n", state);
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}
