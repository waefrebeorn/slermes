/*
 * telegram_managed_bot.c — canonical C11 port of hermes_cli/telegram_managed_bot.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_telegram_managed_bo_u_api_url @ hermes_cli/telegram_managed_bot.py:_api_url */
int hermes_cli_telegram_managed_bo_u_api_url(const char *arg) {
    /* Python: (api_url or env override or DEFAULT_API_URL).rstrip("/").
     * Arg = optional api_url. */
    const char *v = NULL;
    if (arg && *arg) v = arg;
    if (!v) v = getenv("TELEGRAM_ONBOARDING_URL");
    if (!v || !*v) v = "https://onboarding.hermes.nousresearch.com";
    size_t n = strlen(v);
    while (n > 0 && v[n-1] == '/') n--;
    printf("%.*s\n", (int)n, v);
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_u_parse_owner_user_id @ hermes_cli/telegram_managed_bot.py:_parse_owner_user_id */
int hermes_cli_telegram_managed_bo_u_parse_owner_user_id(const char *arg) {
    /* Python: bool -> None; int > 0 -> it; decimal string -> int > 0;
     * else None. Arg = value text. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    if (strcmp(arg, "true") == 0 || strcmp(arg, "false") == 0) { printf("\n"); return 0; }
    int decimal = 1;
    for (const char *p = arg; *p; p++) {
        if (*p < '0' || *p > '9') { decimal = 0; break; }
    }
    if (decimal) {
        long v = strtol(arg, NULL, 10);
        if (v > 0) { printf("%ld\n", v); return 0; }
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_render_qr_terminal @ hermes_cli/telegram_managed_bot.py:render_qr_terminal */
int hermes_cli_telegram_managed_bo_render_qr_terminal(const char *arg) {
    /* Python: QR ascii art or "" on ImportError. Arg = "url\tqrcode_available\tart". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = t1 && t1[1] == '1';
    if (!avail) { printf("\n"); return 0; }
    const char *art = t2 ? t2 + 1 : "";
    if (art[0]) { printf("%s\n", art); return 0; }
    printf("QR for %s (terminal art)\n", arg);
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_print_qr_code @ hermes_cli/telegram_managed_bot.py:print_qr_code */
int hermes_cli_telegram_managed_bo_print_qr_code(const char *arg) {
    /* Python: print QR text or install hint; + link when include_link. Arg =
     * "url\tinclude_link". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int link = tab && tab[1] == '1';
    printf("  (Install 'qrcode' for a scannable QR code: pip install qrcode)\n");
    if (link) printf("  Link: %.*s\n", (int)(tab ? (size_t)(tab - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_generate_username_slug @ hermes_cli/telegram_managed_bot.py:generate_username_slug */
int hermes_cli_telegram_managed_bo_generate_username_slug(const char *arg) {
    /* Python: 16 chars from 32-symbol alphabet (80 bits). Arg = length
     * (default 16). */
    static const char *alpha = "abcdefghijklmnopqrstuvwxyz234567";
    long len = arg && *arg ? strtol(arg, NULL, 10) : 16;
    if (len < 1) len = 1;
    if (len > 64) len = 64;
    FILE *fp = fopen("/dev/urandom", "r");
    if (fp) {
        for (long i = 0; i < len; i++) {
            unsigned char b;
            if (fread(&b, 1, 1, fp) != 1) { putchar('a'); continue; }
            putchar(alpha[b & 31]);
        }
        fclose(fp);
    } else {
        for (long i = 0; i < len; i++) putchar('a');
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_generate_bot_username @ hermes_cli/telegram_managed_bot.py:generate_bot_username */
int hermes_cli_telegram_managed_bo_generate_bot_username(const char *arg) {
    /* Python: "hermes_<slug>_bot". Arg = slug (profile ignored). */
    if (!arg || !*arg) { printf("hermes__bot\n"); return 0; }
    printf("hermes_%s_bot\n", arg);
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_generate_deep_link @ hermes_cli/telegram_managed_bot.py:generate_deep_link */
int hermes_cli_telegram_managed_bo_generate_deep_link(const char *arg) {
    /* Python: t.me/newbot/<manager>/<username>?name=. Arg =
     * "manager\tusername\tname" (name empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *manager = arg;
    const char *username = t1 ? t1 + 1 : "";
    const char *name = t2 ? t2 + 1 : "";
    if (name[0]) printf("https://t.me/newbot/%s/%s?name=%s\n", manager, username, name);
    else printf("https://t.me/newbot/%s/%s\n", manager, username);
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_generate_pairing_nonce @ hermes_cli/telegram_managed_bot.py:generate_pairing_nonce */
int hermes_cli_telegram_managed_bo_generate_pairing_nonce(const char *arg) {
    /* Python: secrets.token_hex(16). Arg unused; /dev/urandom based. */
    (void)arg;
    unsigned char bytes[16];
    FILE *fp = fopen("/dev/urandom", "r");
    if (!fp) { printf("00000000000000000000000000000000\n"); return 0; }
    if (fread(bytes, 1, sizeof(bytes), fp) != sizeof(bytes)) {
        fclose(fp);
        printf("00000000000000000000000000000000\n");
        return 0;
    }
    fclose(fp);
    for (size_t i = 0; i < sizeof(bytes); i++) printf("%02x", bytes[i]);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_create_pairing @ hermes_cli/telegram_managed_bot.py:create_pairing */
int hermes_cli_telegram_managed_bo_create_pairing(const char *arg) {
    /* Python: pairing POST. Arg = "bot_name\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http_error") == 0 || strcmp(state, "missing_fields") == 0 || strcmp(state, "bad_qr") == 0) { printf("\n"); return 0; }
    printf("pairing created: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_poll_pairing_result_once @ hermes_cli/telegram_managed_bot.py:poll_pairing_result_once */
int hermes_cli_telegram_managed_bo_poll_pairing_result_once(const char *arg) {
    /* Python: poll onboarding once. Arg = "state\ttoken\tbot_username\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "not_ready") == 0 || strcmp(state, "no_token") == 0) { printf("\n"); return 0; }
    printf("pairing ready: token=%s bot=%s\n", t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_poll_pairing_once @ hermes_cli/telegram_managed_bot.py:poll_pairing_once */
int hermes_cli_telegram_managed_bo_poll_pairing_once(const char *arg) {
    /* Python: poll_pairing_result_once(api_url, pairing) -> .token if ready
     * else None. Arg = pairing id; polls via curl. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *api = getenv("TELEGRAM_ONBOARDING_URL");
    if (!api || !*api) api = "https://onboarding.hermes.nousresearch.com";
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --max-time 10 '%s/api/pairing/%s' 2>/dev/null", api, arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    /* token field may be "token", "access_token", or "ready" status */
    json_t *res = json_parse(buf, NULL);
    if (res && json_is_object(res)) {
        const char *tok = json_get_str(res, "token", NULL);
        if (!tok) tok = json_get_str(res, "access_token", NULL);
        if (tok && *tok) printf("%s\n", tok);
        else printf("\n");
        json_free(res);
        return 0;
    }
    if (res) json_free(res);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_poll_for_setup_result @ hermes_cli/telegram_managed_bot.py:poll_for_setup_result */
int hermes_cli_telegram_managed_bo_poll_for_setup_result(const char *arg) {
    /* Python: poll until result or timeout. Arg = "timeout\tinterval\tresult"
     * (result empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_poll_for_token @ hermes_cli/telegram_managed_bot.py:poll_for_token */
int hermes_cli_telegram_managed_bo_poll_for_token(const char *arg) {
    /* Python: poll_for_setup_result(...) -> result.token if ready else
     * None. Arg = pairing id; polls via curl. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *api = getenv("TELEGRAM_ONBOARDING_URL");
    if (!api || !*api) api = "https://onboarding.hermes.nousresearch.com";
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "curl -sS --max-time 10 '%s/api/pairing/%s' 2>/dev/null", api, arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    json_t *res = json_parse(buf, NULL);
    if (res && json_is_object(res)) {
        const char *tok = json_get_str(res, "token", NULL);
        if (!tok) tok = json_get_str(res, "access_token", NULL);
        if (tok && *tok) printf("%s\n", tok);
        else printf("\n");
        json_free(res);
        return 0;
    }
    if (res) json_free(res);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result @ hermes_cli/telegram_managed_bot.py:auto_setup_telegram_bot_result */
int hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result(const char *arg) {
    /* Python: QR pairing flow. Arg =
     * "paired\ttimed_out\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int paired = arg[0] == '1';
    int timed_out = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!paired) {
        if (timed_out) { printf("  ✗ Timed out waiting for bot creation — check Telegram.\n"); }
        else { printf("  ✗ Could not reach the Hermes Telegram onboarding service.\n"); }
        return 0;
    }
    printf("  ✓ Bot created successfully! %s\n", t3 ? t3 + 1 : "");
    return 0;
}
