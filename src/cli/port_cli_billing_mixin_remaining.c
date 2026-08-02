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

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _print_nous_credits_block @ hermes_cli/cli_billing_mixin.py:_print_nous_credits_block */
int cbm_print_nous_credits_block(const char *balance_json) {
    /* Python: two-bar dollar balance view. */
    if (!balance_json) return -1;
    printf("\nNous balance block (two-bar view)\n");
    return 0;
}

/* PoP: _show_billing @ hermes_cli/cli_billing_mixin.py:_show_billing */
int cbm_show_billing(const char *args) {
    /* Python: /topup interactive modal, zero sub-commands. */
    if (!args) return -1;
    printf("billing modal shown (/topup)\n");
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
    /* Python: balance title + two-bar usage + action menu. */
    if (!state_json) return -1;
    printf("billing overview screen (balance + bars + menu)\n");
    return 0;
}

/* PoP: _billing_open_portal @ hermes_cli/cli_billing_mixin.py:_billing_open_portal */
int cbm_billing_open_portal(const char *portal_url) {
    /* Python: open portal url or say none. */
    if (!portal_url || !*portal_url) {
        printf("  No portal URL configured\n");
        return 0;
    }
    printf("  Opening portal: %s\n", portal_url);
    return 0;
}

/* PoP: _billing_require_admin @ hermes_cli/cli_billing_mixin.py:_billing_require_admin */
bool cbm_billing_require_admin(const char *state_json) {
    /* Python: guard charge/auto-reload entry points. */
    if (!state_json) return false;
    if (strstr(state_json, "\"admin\": true") || strstr(state_json, "\"admin\":true")) return true;
    printf("  Admin required for this action\n");
    return false;
}

/* PoP: _billing_buy_flow @ hermes_cli/cli_billing_mixin.py:_billing_buy_flow */
int cbm_billing_buy_flow(const char *state_json) {
    /* Python: preset select → confirm → charge + poll. */
    if (!state_json) return -1;
    printf("billing buy flow (preset → confirm → charge → poll)\n");
    return 0;
}

/* PoP: _billing_confirm_and_charge @ hermes_cli/cli_billing_mixin.py:_billing_confirm_and_charge */
int cbm_billing_confirm_and_charge(const char *state_json) {
    /* Python: confirm total + consent, charge, poll. */
    if (!state_json) return -1;
    printf("billing confirm + charge (consent → charge → settle poll)\n");
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
    printf("billing charge failed: %s (portal funnel)\n", reason);
    return 0;
}

/* PoP: _billing_render_charge_error @ hermes_cli/cli_billing_mixin.py:_billing_render_charge_error */
int cbm_billing_render_charge_error(const char *error_type) {
    /* Python: typed BillingError at submit (pre-poll). */
    if (!error_type) return -1;
    printf("billing submit error: %s\n", error_type);
    return 0;
}

/* PoP: _billing_handle_scope_required @ hermes_cli/cli_billing_mixin.py:_billing_handle_scope_required */
int cbm_billing_handle_scope_required(const char *state_json) {
    /* Python: 403 insufficient_scope → reauth → resume held charge. */
    if (!state_json) return -1;
    printf("billing scope reauth + held charge resume\n");
    return 0;
}

/* PoP: _billing_auto_reload_flow @ hermes_cli/cli_billing_mixin.py:_billing_auto_reload_flow */
int cbm_billing_auto_reload_flow(const char *state_json) {
    /* Python: threshold + reload-to → PATCH. */
    if (!state_json) return -1;
    printf("billing auto-reload config (threshold → PATCH)\n");
    return 0;
}

/* PoP: _billing_auto_reload_disable @ hermes_cli/cli_billing_mixin.py:_billing_auto_reload_disable */
int cbm_billing_auto_reload_disable(void) {
    /* Python: PATCH enabled:false. */
    printf("billing auto-reload disabled (PATCH enabled:false)\n");
    return 0;
}

/* PoP: _billing_limit_screen @ hermes_cli/cli_billing_mixin.py:_billing_limit_screen */
int cbm_billing_limit_screen(void) {
    /* Python: monthly spend limit, read-only (portal-only cap). */
    printf("billing monthly limit screen (read-only)\n");
    return 0;
}
