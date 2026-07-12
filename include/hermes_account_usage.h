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

/* Render snapshot to display lines. Name parity.
 * Returns a NULL-terminated array of strings the caller must free.
 * Each line is a separate allocation. */
char **render_account_usage_lines(const account_usage_snapshot_t *snap,
    bool markdown);

/* Free the rendered lines array */
void account_usage_free_lines(char **lines);

/* Port of Python: AccountUsageSnapshot.available (property) */
bool account_usage_available(const account_usage_snapshot_t *snap);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ACCOUNT_USAGE_H */
