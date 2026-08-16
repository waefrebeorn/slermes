/*
 * hermes_account_usage.h — Provider account usage tracking.
 * Port of Python agent/account_usage.py (326 lines).
 *
 * Fetches usage data from provider APIs (OpenAI Codex, Anthropic,
 * OpenRouter) and renders it for display.
 */
#ifndef HERMES_ACCOUNT_USAGE_H
#define HERMES_ACCOUNT_USAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACCOUNT_USAGE_WINDOWS_MAX 8
#define ACCOUNT_USAGE_DETAILS_MAX 4

/* A single usage rate-limit window */
typedef struct {
    char label[64];
    double used_percent;          /* -1.0 = unavailable */
    int64_t reset_at;             /* unix timestamp, 0 = unknown */
    char detail[256];
} account_usage_window_t;

/* Complete account usage snapshot */
typedef struct {
    char provider[32];
    char source[32];
    int64_t fetched_at;           /* unix timestamp */
    char title[128];
    char plan[64];
    account_usage_window_t windows[ACCOUNT_USAGE_WINDOWS_MAX];
    int window_count;
    char details[ACCOUNT_USAGE_DETAILS_MAX][256];
    int detail_count;
    /* AG26: Port of Python agent/account_usage.py:AccountUsageSnapshot.available() */
    bool available;
    char unavailable_reason[256];
} account_usage_snapshot_t;

/* Fetch usage for a provider. Returns NULL on failure or unsupported provider.
 * Supported: "openrouter", "openai-codex", "anthropic".
 * Caller must free the returned snapshot with account_usage_free(). Name parity. */
account_usage_snapshot_t *fetch_account_usage(const char *provider,
    const char *base_url, const char *api_key);

/* Port of Python: agent/account_usage.py:CreditsView (dataclass) */
#define CREDITS_VIEW_BALANCE_LINES_MAX 32
typedef struct {
    bool logged_in;
    char *balance_lines[CREDITS_VIEW_BALANCE_LINES_MAX];
    int balance_line_count;
    char identity_line[512];   /* may be empty */
    char topup_url[512];
    bool depleted;
} credits_view_t;

/* Build the /credits view: balance block + identity line + top-up URL.
 * Fetches the Nous portal account info (network boundary) and renders.
 * Fail-open returns a view with logged_in=false. Caller must free via
 * account_usage_free_credits_view(). */
credits_view_t *account_usage_build_credits_view(bool markdown, double timeout);
void account_usage_free_credits_view(credits_view_t *view);

/* Free a snapshot returned by fetch_account_usage() */
void account_usage_free(account_usage_snapshot_t *snap);

/* === Pure helper ports (agent/account_usage.py) — oracle-verified === */

/* Port of Python: _title_case_slug — "foo_bar-baz" -> "Foo Bar Baz" (returns
 * NULL for empty/unset). Returns a pointer to a static buffer (NOT thread-safe). */
const char *account_usage_title_case_slug(const char *value);

/* Port of Python: _fmt_usd — f"${d:,.2f}" with thousands separators. */
void account_usage_fmt_usd(double d, char *buf, size_t sz);

/* Port of Python: _is_finite_num — real numeric (finite, not NaN/Inf). */
bool account_usage_is_finite_num(double v);

/* Render snapshot to display lines. Name parity.
 * Returns a NULL-terminated array of strings the caller must free.
 * Each line is a separate allocation. */
char **render_account_usage_lines(const account_usage_snapshot_t *snap,
    bool markdown);

/* Free the rendered lines array */
void account_usage_free_lines(char **lines);

/* === Nous portal credits (port of Python agent/account_usage.py) === */

/* Build a usage snapshot from a parsed Nous portal /api/oauth/account JSON.
 * Caller frees with account_usage_free(). Returns NULL on failure. */
account_usage_snapshot_t *account_usage_snapshot_from_credits_state(const json_t *acct);

/* Fetch the Nous portal credits snapshot (network). provider is "nous".
 * Returns NULL on failure (not logged in / no token). */
account_usage_snapshot_t *account_usage_build_nous_credits_snapshot(
    const char *provider, const char *base_url, const char *api_key, double timeout);

/* Render a Nous credits snapshot to display lines, appending the two trailing
 * affordance lines ("Top up: ..." and "(or run /credits)") that the /usage
 * surface adds. Returns a NULL-terminated char** the caller frees with
 * account_usage_free_lines(). */
char **account_usage_nous_credits_lines(const account_usage_snapshot_t *snap,
    bool markdown);

/* === Nous portal auth / recommended model (port of Python auth/account_usage) === */

/* Port of Python _nous_portal_account_has_fresh_paid_access: fetch
 * /api/oauth/account from the portal and return true iff the account
 * currently has paid_service_access. */
bool nous_portal_has_fresh_paid_access(const char *portal_base_url, double timeout);

/* Port of Python _refresh_nous_recommended_model: fetch the portal's
 * recommended model for the account (returns malloc'd model id or NULL).
 * Faithful to hermes_cli.models.fetch_nous_recommended_models. */
char *nous_portal_recommended_model(const char *portal_base_url, double timeout);

/* Port of Python _read_codex_access_token: read a Codex OAuth access token
 * from the stored auth state for provider "codex". Returns malloc'd token or
 * NULL. */
char *read_codex_access_token(void);


/* Port of Python: AccountUsageSnapshot.available (property) */
bool account_usage_available(const account_usage_snapshot_t *snap);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ACCOUNT_USAGE_H */
