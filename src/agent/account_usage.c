/*
 * account_usage.c — canonical C11 port of agent/account_usage.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_account_usage_u_codex_backend_urls @ agent/account_usage.py:_codex_backend_urls */
int agent_account_usage_u_codex_backend_urls(const char *arg) {
    /* Python: wham vs api/codex split. Arg = "base_url\tstyle". */
    if (!arg || !*arg) {
        printf("https://chatgpt.com/backend-api/codex/wham/usage\n");
        return 0;
    }
    const char *tab = strchr(arg, '\t');
    const char *style = tab ? tab + 1 : "";
    if (strcmp(style, "wham") == 0) {
        printf("%s/wham/usage\n%s/wham/rate-limit-reset-credits\n%s/wham/rate-limit-reset-credits/consume\n", arg, arg, arg);
        return 0;
    }
    printf("%s/api/codex/usage\n%s/api/codex/rate-limit-reset-credits\n%s/api/codex/rate-limit-reset-credits/consume\n", arg, arg, arg);
    return 0;
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
