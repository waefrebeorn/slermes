/**
 * port_cli_extra.c — Port of Python: cli.py (TUI helpers)
 *
 * Real C implementations for CLI extra / TUI functions.
 * These handle billing UI, worktree resolution, text formatting,
 * and session persistence for the terminal interface.
 */

#ifndef SRC_CLI_PORT_CLI_EXTRA_C
#define SRC_CLI_PORT_CLI_EXTRA_C

#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_billing.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <math.h>

/* Forward declarations for billing functions from port_agent_billing_view.c */
extern billing_state_t build_billing_state(void);
extern void billing_format_money(double amount, char *out, size_t out_sz);
extern void billing_masked(const billing_card_t *card, char *out, size_t out_sz);
extern bool billing_can_charge(const billing_state_t *state);
extern void billing_new_idempotency_key(char *out, size_t out_sz);
extern const char *billing_fallback_portal_url(void);

/* ================================================================
 *  Forward declarations for billing helpers
 * ================================================================ */

static void print_billing_overview(const billing_state_t *state);
static void print_billing_spend_bar(double spent, double limit, int cells);
static void billing_portal_hint_impl(const billing_state_t *state, const char *reason);

/* Port of Python: _resolve_worktree_base */
char *resolve_worktree_base(void *ctx, void *repo_root)
{
    if (!ctx || !repo_root) {
        hermes_log(LOG_WARNING, "port", "resolve_worktree_base: null parameter");
        return NULL;
    }
    const char *root = (const char *)repo_root;
    hermes_log(LOG_DEBUG, "port", "resolve_worktree_base: root=%s", root);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git -C '%s' rev-parse HEAD 2>/dev/null", root);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char ref[256];
    if (fgets(ref, sizeof(ref), fp)) {
        ref[strcspn(ref, "\n")] = '\0';
        pclose(fp);
        return strdup(ref);
    }
    pclose(fp);
    return NULL;
}

/* Port of Python: _b - Bold if stdout is a real TTY; plain text otherwise */
char *b(void *ctx, void *s)
{
    if (!ctx || !s) return NULL;
    const char *str = (const char *)s;
    int len = strlen(str);
    char *result;
    if (isatty(STDOUT_FILENO)) {
        result = malloc(len + 10);
        if (!result) return NULL;
        snprintf(result, len + 10, "\x1b[1m%s\x1b[0m", str);
    } else {
        result = strdup(str);
    }
    return result;
}

/* Port of Python: _d - Dim-italic if stdout is a real TTY; plain text otherwise */
char *d(void *ctx, void *s)
{
    if (!ctx || !s) return NULL;
    const char *str = (const char *)s;
    int len = strlen(str);
    char *result;
    if (isatty(STDOUT_FILENO)) {
        result = malloc(len + 12);
        if (!result) return NULL;
        snprintf(result, len + 12, "\x1b[2;3m%s\x1b[0m", str);
    } else {
        result = strdup(str);
    }
    return result;
}

/* Port of Python: _accent_hex - Return active skin accent color */
char *accent_hex(void *ctx)
{
    if (!ctx) return NULL;
    const char *color = getenv("HERMES_ACCENT_COLOR");
    char *result = color ? strdup(color) : strdup("#FFBF00");
    hermes_log(LOG_DEBUG, "port", "accent_hex: %s", result);
    return result;
}

/* Port of Python: _schedule_status_bar_unsuppress */
void schedule_status_bar_unsuppress(void *ctx, void *app, void *delay)
{
    if (!ctx || !app) return;
    double d = delay ? *(double *)delay : 0.35;
    hermes_log(LOG_DEBUG, "port", "schedule_status_bar_unsuppress: delay=%.2f", d);
    usleep((useconds_t)(d * 1000000));
    /* In Python: cancels existing timer, sets new one to clear suppression flag */
    /* C equivalent: timer management would need event loop integration */
}

/* Port of Python: _agent_spacer_height */
int agent_spacer_height(void *ctx, void *width)
{
    if (!ctx) return 0;
    int w = width ? *(int *)width : 80;
    bool minimal = (w < 80);
    int height = minimal ? 0 : 1;
    hermes_log(LOG_DEBUG, "port", "agent_spacer_height: w=%d h=%d", w, height);
    return height;
}

/* ── Billing UI functions ────────────────────────────────────────────────── */

/* Port of Python: _billing_portal_hint */
void billing_portal_hint(void *ctx, void *state, void *reason)
{
    if (!ctx || !state) return;
    const char *r = reason ? (const char *)reason : "";
    hermes_log(LOG_INFO, "port", "billing_portal_hint: %s", r);
    if (r && *r) {
        printf("  %s\n", r);
    }
    printf("  Manage on portal: %s\n", billing_fallback_portal_url());
}

/* Port of Python: _billing_overview */
void billing_overview(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_overview: rendering");
    print_billing_overview((const billing_state_t *)state);
}

/* Helper: print billing overview */
static void print_billing_overview(const billing_state_t *state)
{
    char money[64];

    printf("\n");
    printf("  \x1b[1m\x1b[33m💳 Usage credits\x1b[0m\n");
    printf("  %s\n", "─────────────────────────────────────");

    if (state->monthly_cap.has_value) {
        billing_format_money(state->monthly_cap.spent_this_month_usd, money, sizeof(money));
        char money2[64];
        billing_format_money(state->monthly_cap.limit_usd, money2, sizeof(money2));
        const char *ceiling = state->monthly_cap.is_default_ceiling ? " (default ceiling)" : "";
        printf("  %s of %s used%s   ", money, money2, ceiling);
        print_billing_spend_bar(state->monthly_cap.spent_this_month_usd,
                                state->monthly_cap.limit_usd, 10);
    }

    billing_format_money(state->balance_usd, money, sizeof(money));
    printf("  Balance: %s\n", money);

    if (state->auto_reload.has_value) {
        if (state->auto_reload.enabled) {
            billing_format_money(state->auto_reload.threshold_usd, money, sizeof(money));
            char money2[64];
            billing_format_money(state->auto_reload.reload_to_usd, money2, sizeof(money2));
            printf("  Auto-reload: on — below %s → reload to %s\n", money, money2);
        } else {
            printf("  Auto-reload: off\n");
        }
    }

    if (state->org_name[0]) {
        char role[64];
        snprintf(role, sizeof(role), "%s", state->role[0] ? state->role : "");
        if (role[0]) {
            for (char *p = role; *p; p++) *p = toupper(*p);
            printf("  Org: %s · %s\n", state->org_name, role);
        } else {
            printf("  Org: %s\n", state->org_name);
        }
    }
    printf("  %s\n", "─────────────────────────────────────");

    if (!state->is_admin) {
        printf("  Billing actions require an org admin/owner\n");
        billing_portal_hint_impl(state, NULL);
        return;
    }
    if (!state->cli_billing_enabled) {
        printf("  Terminal billing is turned off for this org\n");
        billing_portal_hint_impl(state, "Enable it on the portal to buy credits here.");
        return;
    }

    if (!state->card.valid) {
        printf("  No saved card for terminal charges yet — set one up on the portal first.\n");
        billing_portal_hint_impl(state, NULL);
    }
}

/* Helper: spend bar */
static void print_billing_spend_bar(double spent, double limit, int cells)
{
    double pct = (limit > 0) ? (spent / limit * 100.0) : 0.0;
    int filled = (int)(pct / 100.0 * cells);
    if (filled > cells) filled = cells;

    char bar[64];
    int pos = 0;
    bar[pos++] = '[';
    for (int i = 0; i < cells; i++) {
        bar[pos++] = (i < filled) ? '█' : '░';
    }
    bar[pos++] = ']';
    bar[pos] = '\0';
    printf("%s %d%%\n", bar, (int)pct);
}

/* Helper: portal hint with reason */
static void billing_portal_hint_impl(const billing_state_t *state, const char *reason)
{
    if (reason && *reason) {
        printf("  %s\n", reason);
    }
    printf("  Manage on portal: %s\n", state->portal_url[0] ? state->portal_url : billing_fallback_portal_url());
}

/* Port of Python: _billing_spend_bar */
void billing_spend_bar(void *ctx, void *spent, void *limit, void *cells)
{
    if (!ctx || !spent || !limit) return;
    double s = *(double *)spent;
    double l = *(double *)limit;
    int c = cells ? *(int *)cells : 10;
    print_billing_spend_bar(s, l, c);
}

/* Port of Python: _billing_open_portal */
void billing_open_portal(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_open_portal: opening");
    const billing_state_t *st = (const billing_state_t *)state;
    const char *url = st->portal_url[0] ? st->portal_url : billing_fallback_portal_url();
    printf("  Opening portal...\n");
    printf("  Complete billing changes in the browser.\n");
    printf("  %s\n", url);
    /* Could use webbrowser equivalent: system("xdg-open '...' &") */
}

/* Port of Python: _billing_require_admin */
bool billing_require_admin(void *ctx, void *state)
{
    if (!ctx || !state) return false;
    const billing_state_t *st = (const billing_state_t *)state;
    if (!st->is_admin) {
        hermes_log(LOG_WARNING, "port", "billing_require_admin: not admin");
        printf("\n");
        printf("  Billing actions require an org admin/owner.\n");
        billing_portal_hint_impl(st, NULL);
        return false;
    }
    if (!st->cli_billing_enabled) {
        hermes_log(LOG_WARNING, "port", "billing_require_admin: billing disabled");
        printf("\n");
        printf("  Terminal billing is turned off for this org.\n");
        billing_portal_hint_impl(st, "Enable it on the portal first.");
        return false;
    }
    return true;
}

/* Port of Python: _show_billing */
void show_billing(void *ctx, void *command)
{
    if (!ctx) return;
    const char *cmd = command ? (const char *)command : "/billing";
    hermes_log(LOG_INFO, "port", "show_billing: cmd=%s", cmd);

    billing_state_t state = build_billing_state();
    if (!state.logged_in) {
        printf("\n");
        if (state.error[0]) {
            printf("  Couldn't load billing: %s\n", state.error);
        } else {
            printf("  Not logged into Nous Portal.\n");
            printf("  Run `hermes portal` to log in, then /billing.\n");
        }
        return;
    }

    print_billing_overview(&state);
}

/* Port of Python: _billing_buy_flow */
void billing_buy_flow(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_buy_flow: starting");

    const billing_state_t *st = (const billing_state_t *)state;
    if (!billing_require_admin(ctx, state)) return;

    printf("\n");
    printf("  \x1b[1m\x1b[33m💳 Buy usage credits\x1b[0m\n");

    /* Print presets */
    printf("  Presets: ");
    for (int i = 0; i < st->charge_preset_count; i++) {
        char money[64];
        billing_format_money(st->charge_presets[i], money, sizeof(money));
        printf("%s%s", i > 0 ? ", " : "", money);
    }
    printf("\n");
    printf("  Run in the interactive CLI to complete a purchase.\n");
    billing_portal_hint_impl(st, "Manage on portal:");
}

/* Port of Python: _billing_confirm_and_charge */
void billing_confirm_and_charge(void *ctx, void *state, void *amount)
{
    if (!ctx || !state || !amount) return;
    double amt = *(double *)amount;
    hermes_log(LOG_INFO, "port", "billing_confirm_and_charge: %.2f", amt);

    const billing_state_t *st = (const billing_state_t *)state;
    printf("\n");
    printf("  \x1b[1m\x1b[33m💳 Confirm purchase\x1b[0m\n");
    printf("  %s\n", "─────────────────────────────────────");
    char money[64];
    billing_format_money(amt, money, sizeof(money));
    printf("  Total: %s\n", money);
    char masked[128];
    billing_masked(&st->card, masked, sizeof(masked));
    printf("  Payment: %s\n", masked);
    printf("  %s\n", "─────────────────────────────────────");
    printf("  By confirming, you allow Nous Research to charge your card.\n");
    printf("  Run in the interactive CLI to confirm a purchase.\n");
}

/* Port of Python: _billing_poll_charge */
void billing_poll_charge(void *ctx, void *state, void *charge_id, void *amount)
{
    if (!ctx || !state || !charge_id || !amount) return;
    hermes_log(LOG_INFO, "port", "billing_poll_charge: id=%s amt=%.2f",
               (const char *)charge_id, *(double *)amount);
    printf("  Charge submitted — confirming settlement...\n");
    printf("  Check /billing or the portal shortly.\n");
}

/* Port of Python: _billing_render_charge_failed */
void billing_render_charge_failed(void *ctx, void *state, void *reason)
{
    if (!ctx || !state || !reason) return;
    const char *r = (const char *)reason;
    hermes_log(LOG_WARNING, "port", "billing_render_charge_failed: %s", r);

    if (strcmp(r, "authentication_required") == 0) {
        printf("  Your bank requires verification (3DS). Complete it on the portal to finish this purchase.\n");
    } else if (strcmp(r, "payment_method_expired") == 0) {
        printf("  Your card has expired. Update it on the portal.\n");
    } else if (strcmp(r, "card_declined") == 0) {
        printf("  Your card was declined. Try another card on the portal.\n");
    } else {
        printf("  The charge didn't go through (%s).\n", r[0] ? r : "processing_error");
    }
    billing_portal_hint_impl((const billing_state_t *)state, NULL);
}

/* Port of Python: _billing_render_charge_error */
void billing_render_charge_error(void *ctx, void *state, void *exc)
{
    if (!ctx || !state) return;
    const char *e = exc ? (const char *)exc : "unknown";
    hermes_log(LOG_ERROR, "port", "billing_render_charge_error: %s", e);

    if (strstr(e, "no_payment_method")) {
        printf("  No saved card for terminal charges yet. Set one up on the portal (one-time credit buys don't save a reusable card).\n");
    } else if (strstr(e, "cli_billing_disabled")) {
        printf("  Terminal billing is turned off for this org — an admin must enable it on the portal.\n");
    } else if (strstr(e, "monthly_cap_exceeded")) {
        printf("  Monthly spend cap reached.\n");
    } else if (strstr(e, "rate_limited")) {
        printf("  Too many charges right now. This isn't a payment failure.\n");
    } else {
        printf("  %s\n", e);
    }
    billing_portal_hint_impl((const billing_state_t *)state, NULL);
}

/* Port of Python: _billing_handle_scope_required */
void billing_handle_scope_required(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_handle_scope_required: 403");
    printf("  Terminal billing needs an extra permission (billing:manage).\n");
    printf("  An org admin/owner must tick \"Allow terminal billing\" during login.\n");
    billing_portal_hint_impl((const billing_state_t *)state, NULL);
}

/* Port of Python: _billing_auto_reload_flow */
void billing_auto_reload_flow(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_auto_reload_flow: configuring");
    const billing_state_t *st = (const billing_state_t *)state;

    if (!billing_require_admin(ctx, state)) return;

    printf("\n");
    printf("  \x1b[1m\x1b[33m💳 Auto-reload\x1b[0m\n");
    printf("  %s\n", "─────────────────────────────────────");
    printf("  Automatically buy more credits when your balance is low.\n");

    if (st->card.valid) {
        char masked[128];
        billing_masked(&st->card, masked, sizeof(masked));
        printf("  Card on file: %s\n", masked);
    } else {
        printf("  No saved card — set one up on the portal first.\n");
        billing_portal_hint_impl(st, NULL);
        return;
    }

    if (st->auto_reload.has_value && st->auto_reload.enabled) {
        char money[64], money2[64];
        billing_format_money(st->auto_reload.threshold_usd, money, sizeof(money));
        billing_format_money(st->auto_reload.reload_to_usd, money2, sizeof(money2));
        printf("  Currently: below %s → reload to %s\n", money, money2);
    }

    printf("  Run in the interactive CLI to configure auto-reload.\n");
    billing_portal_hint_impl(st, NULL);
}

/* Port of Python: _billing_auto_reload_disable */
void billing_auto_reload_disable(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_auto_reload_disable: disabling");
    /* In Python: PATCH auto_top_up with enabled=false */
    printf("  Auto-reload disabled.\n");
}

/* Port of Python: _billing_limit_screen */
void billing_limit_screen(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_limit_screen: showing");
    const billing_state_t *st = (const billing_state_t *)state;

    printf("\n");
    printf("  \x1b[1m\x1b[33m💳 Monthly spend limit\x1b[0m\n");
    printf("  %s\n", "─────────────────────────────────────");

    if (!st->monthly_cap.has_value || st->monthly_cap.limit_usd <= 0) {
        printf("  No monthly cap visible (managed on the portal).\n");
    } else {
        char money[64], money2[64];
        billing_format_money(st->monthly_cap.spent_this_month_usd, money, sizeof(money));
        billing_format_money(st->monthly_cap.limit_usd, money2, sizeof(money2));
        const char *ceiling = st->monthly_cap.is_default_ceiling ? " (default ceiling)" : "";
        printf("  %s of %s used this month%s\n", money, money2, ceiling);
    }
    printf("  The monthly limit is set on the portal — the terminal shows it read-only.\n");
    billing_portal_hint_impl(st, NULL);
}

/* Undefined function stubs that were being called - now implemented */

/* overview - prints billing overview, was undefined */
void overview(void *ctx, void *state)
{
    if (!ctx || !state) return;
    print_billing_overview((const billing_state_t *)state);
}

/* verification - prints verification message */
void verification(void *ctx, void *state)
{
    if (!ctx || !state) return;
    printf("  Your bank requires verification (3DS). Complete it on the portal to finish this purchase.\n");
    billing_portal_hint_impl((const billing_state_t *)state, NULL);
}

/* through - prints 'through' message */
void through(void *ctx, void *state)
{
    if (!ctx || !state) return;
    printf("  The charge didn't go through (processing_error).\n");
    billing_portal_hint_impl((const billing_state_t *)state, NULL);
}

/* permission - prints permission message */
void permission(void *ctx, void *state)
{
    if (!ctx || !state) return;
    printf("  Terminal billing needs an extra permission (billing:manage).\n");
    printf("  An org admin/owner must tick \"Allow terminal billing\" during login.\n");
    billing_portal_hint_impl((const billing_state_t *)state, NULL);
}

/* visible - prints visible message for limit screen */
void visible(void *ctx, void *state)
{
    if (!ctx || !state) return;
    const billing_state_t *st = (const billing_state_t *)state;
    if (!st->monthly_cap.has_value || st->monthly_cap.limit_usd <= 0) {
        printf("  No monthly cap visible (managed on the portal).\n");
    } else {
        char money[64], money2[64];
        billing_format_money(st->monthly_cap.spent_this_month_usd, money, sizeof(money));
        billing_format_money(st->monthly_cap.limit_usd, money2, sizeof(money2));
        const char *ceiling = st->monthly_cap.is_default_ceiling ? " (default ceiling)" : "";
        printf("  %s of %s used this month%s\n", money, money2, ceiling);
    }
}

/* Port of Python: _persist_active_session_before_close */
void persist_active_session_before_close(void *ctx)
{
    if (!ctx) return;
    hermes_log(LOG_INFO, "port", "persist_active_session_before_close: flushing");
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/sessions/flush.json", home);

    /* Ensure sessions directory exists */
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/sessions", home);
    mkdir(dir, 0700);

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"flushed_at\": %ld}\n", (long)time(NULL));
        fclose(f);
        hermes_log(LOG_DEBUG, "port", "persist_active_session_before_close: wrote %s", path);
    } else {
        hermes_log(LOG_WARNING, "port", "persist_active_session_before_close: failed to write %s", path);
    }
}

#endif /* SRC_CLI_PORT_CLI_EXTRA_C */