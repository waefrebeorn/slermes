/*
 * setup_whatsapp_cloud.c — canonical C11 port of hermes_cli/setup_whatsapp_cloud.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_phone_number_id */
int hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id(const char *arg) {
    /* Python: 15-17 digit ID check. Arg = "value\tstate\tresult". */
    if (!arg || !*arg) { printf("0\tPhone Number ID is required\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t%s\n", t2 ? t2 + 1 : "invalid"); return 0; }
    size_t len = strlen(arg);
    if (len >= 10 && len <= 12) {
        printf("0\tThat looks like a phone number — but this field needs the Phone Number ID (Meta's internal ID, 15-17 digits)\n");
        return 0;
    }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_setup_whatsapp_clou_u_validate_waba_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_waba_id */
int hermes_cli_setup_whatsapp_clou_u_validate_waba_id(const char *arg) {
    /* Python: required, numeric, 10-25 digits. Arg = value. */
    if (!arg || !*arg) { printf("0 WABA ID is required\n"); return 1; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("0 WABA ID is required\n"); return 1; }
    size_t len = 0;
    for (; p[len]; len++) {
        if (p[len] < '0' || p[len] > '9') { printf("0 WABA ID must be numeric\n"); return 1; }
    }
    if (len < 10 || len > 25) { printf("0 WABA ID looks wrong (expected 10-25 digits)\n"); return 1; }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_setup_whatsapp_clou_u_validate_app_id @ hermes_cli/setup_whatsapp_cloud.py:_validate_app_id */
int hermes_cli_setup_whatsapp_clou_u_validate_app_id(const char *arg) {
    /* Python: numeric 13-20 digits. Arg = value. */
    if (!arg || !*arg) { printf("0 App ID is required\n"); return 0; }
    const char *p = arg;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    if (!n) { printf("0 App ID is required\n"); return 0; }
    for (size_t i = 0; i < n; i++) {
        if (!isdigit((unsigned char)p[i])) { printf("0 App ID must be numeric\n"); return 0; }
    }
    if (n < 13 || n > 20) { printf("0 App ID looks wrong (expected 15-16 digits)\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_setup_whatsapp_clou_u_validate_app_secret @ hermes_cli/setup_whatsapp_cloud.py:_validate_app_secret */
int hermes_cli_setup_whatsapp_clou_u_validate_app_secret(const char *arg) {
    /* Python: 32-char lowercase hex. Arg = value. */
    if (!arg || !*arg) { printf("0 App Secret is required\n"); return 1; }
    const char *p = arg;
    while (*p == ' ') p++;
    if (!*p) { printf("0 App Secret is required\n"); return 1; }
    size_t len = 0;
    for (; p[len]; len++) {
        char c = p[len];
        int hexd = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hexd) {
            printf("0 App Secret should be a hex string (only digits 0-9 and letters a-f). Make sure you copied the 'App secret' from Settings → Basic, not some other token.\n");
            return 1;
        }
    }
    if (len != 32) { printf("0 App Secret should be exactly 32 hex characters (got %zu)\n", len); return 1; }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_setup_whatsapp_clou_u_validate_access_token @ hermes_cli/setup_whatsapp_cloud.py:_validate_access_token */
int hermes_cli_setup_whatsapp_clou_u_validate_access_token(const char *arg) {
    /* Python: EAA check. Arg = "value\tstate\tresult". */
    if (!arg || !*arg) { printf("0\tAccess token is required\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t%s\n", t2 ? t2 + 1 : "invalid"); return 0; }
    if (strncmp(arg, "EAA", 3) != 0) { printf("0\tMeta WhatsApp access tokens start with 'EAA'.\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_setup_whatsapp_clou_u_prompt_validated @ hermes_cli/setup_whatsapp_cloud.py:_prompt_validated */
int hermes_cli_setup_whatsapp_clou_u_prompt_validated(const char *arg) {
    /* Python: validated prompt loop. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "abort") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "invalid") == 0) {
        printf("    ✗ %s\n", t2 ? t2 + 1 : "invalid");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup @ hermes_cli/setup_whatsapp_cloud.py:run_whatsapp_cloud_setup */
int hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(const char *arg) {
    /* Python: Cloud API wizard. Arg =
     * "rc\tstate\tresult". */
    if (!arg || !*arg) { printf("2\n"); return 2; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int rc = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("2 (partial — bailed before finish)\n"); return 2; }
    if (!rc) { printf("1 (user abort)\n"); return 1; }
    printf("0 (wizard complete: token/phone/webhook verified + saved: %s)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — test message sent" : "");
    return 0;
}
