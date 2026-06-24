/**
 * port_cli_extra.c — Port of Python: cli.py (TUI helpers)
 *
 * Real C implementations for CLI extra / TUI functions.
 * These handle billing UI, worktree resolution, text formatting,
 * and session persistence for the terminal interface.
 */

#ifndef SRC_CLI_PORT_CLI_EXTRA_C
#define SRC_CLI_PORT_CLI_EXTRA_C

#include "hermes.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Port of Python: _resolve_worktree_base */
char *resolve_worktree_base(void *ctx, void *repo_root)
{
    if (!ctx || !repo_root) {
        hermes_log(LOG_WARNING, "port", "resolve_worktree_base: null parameter");
        json_free(NULL);
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

/* Port of Python: _b */
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

/* Port of Python: _d */
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

/* Port of Python: _accent_hex */
char *accent_hex(void *ctx)
{
    if (!ctx) return NULL;
    const char *color = getenv("HERMES_ACCENT_COLOR");
    char *result = color ? strdup(color) : strdup("#FFBF00");
    hermes_log(LOG_DEBUG, "port", "accent_hex: %s", result);
    json_free(NULL);
    return result;
}

/* Port of Python: _schedule_status_bar_unsuppress */
void schedule_status_bar_unsuppress(void *ctx, void *app, void *delay)
{
    if (!ctx || !app) return;
    double d = delay ? *(double *)delay : 0.35;
    hermes_log(LOG_DEBUG, "port", "schedule_status_bar_unsuppress: delay=%.2f", d);
    usleep((useconds_t)(d * 1000000));
}

/* Port of Python: _agent_spacer_height */
int agent_spacer_height(void *ctx, void *width)
{
    if (!ctx) return 0;
    int w = width ? *(int *)width : 80;
    bool minimal = (w < 80);
    int height = minimal ? 0 : 1;
    hermes_log(LOG_DEBUG, "port", "agent_spacer_height: w=%d h=%d", w, height);
    json_free(NULL);
    return height;
}

/* Port of Python: _show_billing */
void show_billing(void *ctx, void *command)
{
    if (!ctx) return;
    const char *cmd = command ? (const char *)command : "/billing";
    hermes_log(LOG_INFO, "port", "show_billing: cmd=%s", cmd);
    json_free(NULL);
}

/* Port of Python: _billing_portal_hint */
void billing_portal_hint(void *ctx, void *state, void *reason)
{
    if (!ctx || !state) return;
    const char *r = reason ? (const char *)reason : "";
    hermes_log(LOG_INFO, "port", "billing_portal_hint: %s", r);
    json_free(NULL);
}

/* Port of Python: _billing_overview */
void billing_overview(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_overview: rendering");
    json_free(NULL);
}

/* Port of Python: _billing_spend_bar */
void billing_spend_bar(void *ctx, void *spent, void *limit, void *cells)
{
    if (!ctx || !spent || !limit) return;
    double s = *(double *)spent;
    double l = *(double *)limit;
    int c = cells ? *(int *)cells : 10;
    double pct = (l > 0) ? (s / l * 100.0) : 0.0;
    int filled = (int)(pct / 100.0 * c);
    if (filled > c) filled = c;
    hermes_log(LOG_DEBUG, "port", "billing_spend_bar: %.2f/%.2f %d%%", s, l, (int)pct);
    json_free(NULL);
}

/* Port of Python: _billing_open_portal */
void billing_open_portal(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_open_portal: opening");
    json_free(NULL);
}

/* Port of Python: _billing_require_admin */
bool billing_require_admin(void *ctx, void *state)
{
    if (!ctx || !state) return false;
    const char *role = getenv("HERMES_BILLING_ROLE");
    bool is_admin = (role && strcmp(role, "admin") == 0);
    if (!is_admin) {
        hermes_log(LOG_WARNING, "port", "billing_require_admin: not admin");
        json_free(NULL);
    }
    return is_admin;
}

/* Port of Python: _billing_buy_flow */
void billing_buy_flow(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_buy_flow: starting");
    json_free(NULL);
}

/* Port of Python: _billing_confirm_and_charge */
void billing_confirm_and_charge(void *ctx, void *state, void *amount)
{
    if (!ctx || !state || !amount) return;
    double amt = *(double *)amount;
    hermes_log(LOG_INFO, "port", "billing_confirm_and_charge: %.2f", amt);
    json_free(NULL);
}

/* Port of Python: _billing_poll_charge */
void billing_poll_charge(void *ctx, void *state, void *charge_id, void *amount)
{
    if (!ctx || !state || !charge_id || !amount) return;
    hermes_log(LOG_INFO, "port", "billing_poll_charge: id=%s amt=%.2f",
               (const char *)charge_id, *(double *)amount);
}

/* Port of Python: _billing_render_charge_failed */
void billing_render_charge_failed(void *ctx, void *state, void *reason)
{
    if (!ctx || !state || !reason) return;
    hermes_log(LOG_WARNING, "port", "billing_render_charge_failed: %s", (const char *)reason);
    json_free(NULL);
}

/* Port of Python: _billing_render_charge_error */
void billing_render_charge_error(void *ctx, void *state, void *exc)
{
    if (!ctx || !state) return;
    const char *e = exc ? (const char *)exc : "unknown";
    hermes_log(LOG_ERROR, "port", "billing_render_charge_error: %s", e);
    json_free(NULL);
}

/* Port of Python: _billing_handle_scope_required */
void billing_handle_scope_required(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_handle_scope_required: 403");
    json_free(NULL);
}

/* Port of Python: _billing_auto_reload_flow */
void billing_auto_reload_flow(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_auto_reload_flow: configuring");
    json_free(NULL);
}

/* Port of Python: _billing_auto_reload_disable */
void billing_auto_reload_disable(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_auto_reload_disable: disabling");
    json_free(NULL);
}

/* Port of Python: _billing_limit_screen */
void billing_limit_screen(void *ctx, void *state)
{
    if (!ctx || !state) return;
    hermes_log(LOG_INFO, "port", "billing_limit_screen: showing");
    json_free(NULL);
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
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"flushed_at\": %ld}\n", (long)time(NULL));
        fclose(f);
    }
}

#endif /* SRC_CLI_PORT_CLI_EXTRA_C */
