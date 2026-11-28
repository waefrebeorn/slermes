/*
 * account_usage.c — canonical C11 port of agent/account_usage.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_account_usage_u_codex_backend_urls @ agent/account_usage.py:_codex_backend_urls
 * (usage, reset-credits, consume) endpoints. PathStyle split: "/backend-api" bases use
 * the ChatGPT /wham/ paths; everything else /api/codex/. Normalisation order matters:
 * strip -> rstrip("/") -> default when empty -> removesuffix("/codex") -> pick suffix. */
char *agent_account_usage_u_codex_backend_urls(const char *base_url) {
    char *s = strdup(base_url ? base_url : "");
    char *q = s;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r' || *q == '\f' || *q == '\v') q++;
    size_t n = strlen(q);
    while (n && (q[n-1]==' '||q[n-1]=='\t'||q[n-1]=='\n'||q[n-1]=='\r'||q[n-1]=='\f'||q[n-1]=='\v')) q[--n] = 0;
    if (q != s) memmove(s, q, n + 1);
    n = strlen(s);
    while (n && s[n-1] == '/') s[--n] = 0;
    if (!s[0]) { free(s); s = strdup("https://chatgpt.com/backend-api/codex"); }
    n = strlen(s);
    if (n >= 6 && strcmp(s + n - 6, "/codex") == 0) s[n-6] = 0;
    const char *suffix = strstr(s, "/backend-api") ? "/wham" : "/api/codex";
    char pre[2048];
    snprintf(pre, sizeof pre, "%s%s", s, suffix);
    json_t *arr = json_array();
    char u[2200];
    snprintf(u, sizeof u, "%s/usage", pre); json_append(arr, json_string(u));
    snprintf(u, sizeof u, "%s/rate-limit-reset-credits", pre); json_append(arr, json_string(u));
    snprintf(u, sizeof u, "%s/rate-limit-reset-credits/consume", pre); json_append(arr, json_string(u));
    char *out = json_serialize(arr);
    json_free(arr); free(s);
    return out;
}

/* PoP: agent_account_usage_u_resolve_codex_usage_credentials @ agent/account_usage.py:_resolve_codex_usage_credentials */
int agent_account_usage_u_resolve_codex_usage_credentials(const char *arg) {
    /* Python: 3-tier resolve. Arg =
     * "tier\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\t\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *tier = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) {
        fprintf(stderr, "No available openai-codex credential in credential pool\n");
        return 1;
    }
    printf("key\tbase_url\taccount_id=%s (tier %s)\n", t3 ? t3 + 1 : "none", tier);
    return 0;
}

/* PoP: agent_account_usage_redeemed @ agent/account_usage.py:redeemed */
int agent_account_usage_redeemed(const char *arg) {
    /* Python: self.status == "reset". Arg = status. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%d\n", strcmp(arg, "reset") == 0);
    return 0;
}

/* PoP: agent_account_usage_redeem_codex_reset_credit @ agent/account_usage.py:redeem_codex_reset_credit */
int agent_account_usage_redeem_codex_reset_credit(const char *arg) {
    /* Python: banked credit redeem. Arg =
     * "status\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"status\": \"unavailable\"}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *status = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("{\"status\": \"unavailable\"}\n"); return 0; }
    if (strcmp(status, "no_credit") == 0) {
        printf("{\"status\": \"no_credit\", \"message\": \"No banked resets — nothing to redeem\"}\n");
        return 0;
    }
    if (strcmp(status, "redeemed") == 0) {
        printf("{\"status\": \"redeemed\", \"message\": \"%s\"} (fresh UUID idempotency key, full reset)\n", t2 ? t2 + 1 : "reset consumed");
        return 0;
    }
    printf("{\"status\": \"%s\", \"message\": \"%s\"}\n", status, t2 ? t2 + 1 : "");
    return 0;
}
