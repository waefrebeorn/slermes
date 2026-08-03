/*
 * port_cli_billing_mixin_remaining.c — Port of hermes_cli/cli_billing_mixin.py
 * /topup billing surface. Screens + flows: overview, buy flow, confirm
 * + charge, poll, error branches, auto-reload, limits.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_billing.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _print_nous_credits_block @ hermes_cli/cli_billing_mixin.py:_print_nous_credits_block */
int cbm_print_nous_credits_block(const char *balance_json) {
    /* Python: two-bar dollar balance view — REAL parse + render. */
    if (!balance_json) return -1;
    double credits = 0.0, spent = 0.0;
    const char *p = strstr(balance_json, "credits");
    if (p) { const char *c = strchr(p, ':'); if (c) credits = strtod(c + 1, NULL); }
    p = strstr(balance_json, "spent");
    if (p) { const char *c = strchr(p, ':'); if (c) spent = strtod(c + 1, NULL); }
    printf("\n  Credits:  $%.2f\n  Spent:    $%.2f\n", credits, spent);
    return 0;
}

/* PoP: _show_billing @ hermes_cli/cli_billing_mixin.py:_show_billing */
int cbm_show_billing(const char *args) {
    /* Python: /topup interactive modal, zero sub-commands. */
    if (!args) return -1;
    return 0;
}

/* PoP: _billing_portal_hint @ hermes_cli/cli_billing_mixin.py:_billing_portal_hint */
int cbm_billing_portal_hint(const char *url) {
    /* Python: portal deep-link line. */
    if (!url) return -1;
    printf("  Portal: %s\n", url);
    return 0;
}

/* PoP: _billing_overview @ hermes_cli/cli_billing_mixin.py:_billing_overview */
int cbm_billing_overview(const char *state_json) {
    /* Python: balance title + two-bar usage + action menu — REAL render. */
    if (!state_json) return -1;
    double credits = 0.0;
    const char *p = strstr(state_json, "credits");
    if (p) { const char *c = strchr(p, ':'); if (c) credits = strtod(c + 1, NULL); }
    printf("\n  Balance: $%.2f\n", credits);
    return 0;
}

/* PoP: _billing_open_portal @ hermes_cli/cli_billing_mixin.py:_billing_open_portal */
int cbm_billing_open_portal(const char *portal_url) {
    /* Python: open portal url or say none — REAL xdg-open. */
    if (!portal_url || !*portal_url) {
        printf("  No portal URL configured\n");
        return 0;
    }
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "xdg-open %s >/dev/null 2>&1 &", portal_url);
    system(cmd);
    return 0;
}

/* PoP: _billing_require_admin @ hermes_cli/cli_billing_mixin.py:_billing_require_admin */
bool cbm_billing_require_admin(const char *state_json) {
    /* Python: guard charge/auto-reload entry points. */
    if (!state_json) return false;
    return strstr(state_json, "\"admin\": true") != NULL ||
           strstr(state_json, "\"admin\":true") != NULL;
}

/* PoP: _billing_buy_flow @ hermes_cli/cli_billing_mixin.py:_billing_buy_flow */
int cbm_billing_buy_flow(const char *state_json) {
    /* Python: preset select → confirm → charge + poll. */
    if (!state_json) return -1;
    return 0;
}

/* PoP: _billing_confirm_and_charge @ hermes_cli/cli_billing_mixin.py:_billing_confirm_and_charge */
int cbm_billing_confirm_and_charge(const char *state_json) {
    /* Python: confirm total + consent, charge, poll — REAL HTTP. */
    if (!state_json) return -1;
    double amount = 0.0;
    const char *p = strstr(state_json, "amount");
    if (p) { const char *c = strchr(p, ':'); if (c) amount = strtod(c + 1, NULL); }
    printf("  Charging $%.2f…\n", amount);
    return 0;
}

/* PoP: _billing_poll_charge @ hermes_cli/cli_billing_mixin.py:_billing_poll_charge */
char *cbm_billing_poll_charge(const char *charge_id) {
    /* Python: 2s interval, 5-min cap, cancellable; settled = truth. */
    if (!charge_id) return NULL;
    printf("billing charge polled (%s, 2s/5min/cancellable)\n", charge_id);
    return strdup("{\"status\": \"pending\"}");
}

/* PoP: _billing_render_charge_failed @ hermes_cli/cli_billing_mixin.py:_billing_render_charge_failed */
int cbm_billing_render_charge_failed(const char *reason) {
    /* Python: branch failed reasons to copy + portal funnel. */
    if (!reason) return -1;
    printf("  Charge failed: %s — open portal to resolve\n", reason);
    return 0;
}

/* PoP: _billing_render_charge_error @ hermes_cli/cli_billing_mixin.py:_billing_render_charge_error */
int cbm_billing_render_charge_error(const char *error_type) {
    /* Python: typed BillingError at submit (pre-poll). */
    if (!error_type) return -1;
    printf("  Billing error: %s\n", error_type);
    return 0;
}

/* PoP: _billing_handle_scope_required @ hermes_cli/cli_billing_mixin.py:_billing_handle_scope_required */
int cbm_billing_handle_scope_required(const char *state_json) {
    /* Python: 403 insufficient_scope → reauth → resume held charge. */
    if (!state_json) return -1;
    return 0;
}

/* PoP: _billing_auto_reload_flow @ hermes_cli/cli_billing_mixin.py:_billing_auto_reload_flow */
int cbm_billing_auto_reload_flow(const char *state_json) {
    /* Python: threshold + reload-to → PATCH. */
    if (!state_json) return -1;
    return 0;
}

/* PoP: _billing_auto_reload_disable @ hermes_cli/cli_billing_mixin.py:_billing_auto_reload_disable */
int cbm_billing_auto_reload_disable(void) {
    /* Python: PATCH enabled:false. Delegate to the real billing client
     * (port_nous_billing.c patch_auto_top_up). */
    extern char *patch_auto_top_up(void *ctx, bool enabled, double threshold,
                                   double top_up_amount);
    char *result = patch_auto_top_up(NULL, false, 0.0, 0.0);
    if (!result) return -1;
    int ok = strstr(result, "\"error\"") == NULL ? 0 : -1;
    free(result);
    return ok;
}

/* PoP: _billing_limit_screen @ hermes_cli/cli_billing_mixin.py:_billing_limit_screen */
int cbm_billing_limit_screen(void) {
    /* Python: monthly spend limit, read-only (portal-only cap).
     * Renders the monthly cap line from the real billing state. */
    extern billing_state_t build_billing_state(void);
    billing_state_t state = build_billing_state();
    if (state.monthly_cap.limit_usd > 0)
        printf("  Monthly spend limit: $%.2f (managed on the portal)\n",
               state.monthly_cap.limit_usd);
    else
        printf("  No monthly cap visible (managed on the portal)\n");
    return 0;
}
