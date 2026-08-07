/**
 * hermes_billing.h — Billing subsystem API
 *
 * Defines types and functions for billing state, HTTP calls, and UI.
 */

#ifndef HERMES_BILLING_H
#define HERMES_BILLING_H

#include "hermes_core_types.h"
#include "hermes_json.h"

/* ================================================================
 *  Billing Data Structures
 * ================================================================ */

typedef struct {
    char brand[64];
    char last4[8];
    bool valid;
} billing_card_t;

typedef struct {
    double limit_usd;
    double spent_this_month_usd;
    bool is_default_ceiling;
    bool has_value;
} billing_monthly_cap_t;

typedef struct {
    bool enabled;
    double threshold_usd;
    double reload_to_usd;
    bool has_value;
} billing_auto_reload_t;

typedef struct {
    bool logged_in;
    char org_id[128];
    char org_slug[128];
    char org_name[256];
    char role[32];
    double balance_usd;
    bool cli_billing_enabled;
    double charge_presets[16];
    int charge_preset_count;
    double min_usd;
    double max_usd;
    billing_card_t card;
    billing_monthly_cap_t monthly_cap;
    billing_auto_reload_t auto_reload;
    char portal_url[512];
    char error[512];
    bool is_admin;  /* derived: role is OWNER or ADMIN */
} billing_state_t;

/* ================================================================
 *  Parsing & State Construction
 * ================================================================ */

/* Parse raw JSON payload into billing_state_t */
billing_state_t billing_state_from_payload(const char *payload_json, const char *portal_url);

/* Fetch billing state from server (fail-open) */
billing_state_t build_billing_state(void);

/* ================================================================
 *  State Queries
 * ================================================================ */

/* Check if current state allows charging */
bool billing_can_charge(const billing_state_t *state);

/* ================================================================
 *  Formatting & Display
 * ================================================================ */

/* Format money: $100 or $100.50 */
void billing_format_money(double amount, char *out, size_t out_sz);

/* Format card as "Brand ****1234" */
void billing_masked(const billing_card_t *card, char *out, size_t out_sz);

/* ================================================================
 *  Utilities
 * ================================================================ */

/* Generate new idempotency key for charge */
void billing_new_idempotency_key(char *out, size_t out_sz);

/* Parse money string (strips $ and spaces) */
double billing_parse_money(const char *str);

/* Validate charge amount (positive, within bounds, max 2dp) */
bool billing_validate_charge_amount(double amount, double min_usd, double max_usd);

/* Fallback portal URL */
const char *billing_fallback_portal_url(void);

/* ================================================================
 *  HTTP API (from port_nous_billing.c)
 * ================================================================ */

/* Resolve portal base URL */
char *resolve_portal_base_url(void *ctx, void *state);

/* Get retry-after seconds */
int retry_after_seconds(void *ctx);

/* Fetch billing state JSON */
char *get_billing_state(void *ctx, double timeout);

/* Post a charge */
char *post_charge(void *ctx, double amount_usd, const char *idempotency_key);

/* Get charge status */
char *get_charge_status(void *ctx, const char *charge_id);

/* Patch auto top-up config */
char *patch_auto_top_up(void *ctx, bool enabled, double threshold, double top_up_amount);

/* Absolutize portal URL */
char *absolutize_portal_url(void *ctx, const char *raw_portal_url);

/* Step up OAuth scope for billing */
bool step_up_nous_billing_scope(void *ctx, bool open_browser);

/* ================================================================
 *  Shared raw-JSON text scrapers (used by billing port modules)
 *  Defined in src/cli/billing_json_helpers.c. These parse flat
 *  key/value pairs out of a raw JSON *text* string (the billing
 *  portal returns unstructured JSON we only need a few scalars from).
 * ================================================================ */

/* Extract a numeric value for `key` from raw JSON text. Returns 0.0 if absent. */
double billing_json_get_number(const char *json, const char *key);

/* Extract a boolean (true/false) value for `key` from raw JSON text. */
bool billing_json_get_bool(const char *json, const char *key);

/* Extract a (malloc'd) string value for `key` from raw JSON text.
 * Returns NULL if absent or not a quoted string. Caller frees. */
char *billing_json_get_string(const char *json, const char *key);

/* ================================================================
 *  Billing auth helpers (Port of hermes_cli/nous_billing.py)
 * ================================================================ */

/* Read the current Nous access token into buf (real resolver: env override
 * then auth.json provider state). Returns 0 on success, -1 if none. */
int cli_tools_managed_tool_gateway_peek_nous_access_token(char *buf, size_t bufsize);

/* Read-and-refresh variant: resolves a token, refreshing if needed.
 * Returns 0 on success, -1 if none. */
int cli_tools_managed_tool_gateway_read_nous_access_token(char *buf, size_t bufsize);

/* Build the canonical "not logged in" billing auth error as malloc'd JSON
 * (status=401, error="invalid_token"). Caller frees. */
char *billing_not_logged_in_json(void);

/* Resolve (access_token, portal_base_url). token_out/base_out are malloc'd
 * (caller frees). Returns 0 on success, -1 if no usable Nous session. */
int billing_resolve_token_and_base(char **token_out, char **base_out, bool use_cache);

/* Map an HTTP error response to the right typed billing error as malloc'd
 * JSON (status -> auth/scope/rate_limited/generic). Caller frees. */
char *billing_raise_for_error(int status, const char *payload_json, const char *headers);

#endif /* HERMES_BILLING_H */