/* billing_usage.h — opaque interface for the faithful C11 port of
 * hermes-agent/agent/billing_usage.py
 *
 * Shared dollar-denominated usage model for the billing/subscription surfaces.
 * Pure logic (no IO/network): parses an already-fetched account-info JSON
 * object into a structured UsageModel. The data source is the NAS
 * account-info fetch, which arrives as JSON, so usage_model_from_account()
 * takes a json_t* (reusing libjson) — matching the real data flow.
 *
 * Opaque structs: usage_bar_t / usage_model_t are defined only in the .c;
 * consumers use the accessors below. Self-contained; minimal includes.
 */

#ifndef SLERMES_BILLING_USAGE_H
#define SLERMES_BILLING_USAGE_H

#include <stdbool.h>
#include <stddef.h>   /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Below this TOTAL spendable ($), a paid account is flagged "low". */
#define LOW_BALANCE_THRESHOLD_USD 5.0

/* Coerce a JSON number (or any value) to a finite double, or return false if
 * it is missing / non-numeric / non-finite. Mirrors Python _finite(). */
bool billing_usage_finite(const void *json_value, double *out);

/* "$X.YY" for display. Non-finite / missing -> "$0.00". */
void billing_usage_fmt_usd(double value, char *buf, size_t sz);

/* Format an ISO date/timestamp as "Jul 24, 2026". Returns a malloc'd string
 * (caller frees) or NULL for empty input; returns the raw text for unparsable
 * input (never raises). */
char *billing_usage_format_renews(const char *value);

/* --- Opaque usage bar (one full-resolution bar: spent of total) --- */
typedef struct usage_bar usage_bar_t;

usage_bar_t *usage_bar_make(const char *kind, double remaining, double total,
                            double spent);
void         usage_bar_free(usage_bar_t *b);
const char  *usage_bar_kind(const usage_bar_t *b);
double       usage_bar_remaining(const usage_bar_t *b);
double       usage_bar_total(const usage_bar_t *b);
double       usage_bar_spent(const usage_bar_t *b);
/* pct_used: only for kind=="plan" with total>0; else returns false (no pct). */
bool         usage_bar_pct_used(const usage_bar_t *b, int *out_pct);
/* fill_fraction: remaining/total clamped to [0,1]; 0.0 if total<=0. */
double       usage_bar_fill_fraction(const usage_bar_t *b);

/* --- Opaque usage model (surface-agnostic) --- */
typedef struct usage_model usage_model_t;

/* Build a UsageModel from a parsed account-info JSON object (NousPortalAccountInfo).
 * Fail-open: NULL / not-logged-in / parse error -> available=false. Caller frees. */
usage_model_t *usage_model_from_account(const void *account_info_json);

void   usage_model_free(usage_model_t *m);
bool   usage_model_available(const usage_model_t *m);
const char *usage_model_status(const usage_model_t *m);          /* "free"|"low"|"healthy"|"depleted" */
const char *usage_model_plan_name(const usage_model_t *m);
const char *usage_model_renews_at(const usage_model_t *m);
const char *usage_model_renews_display(const usage_model_t *m);
double usage_model_subscription_remaining(const usage_model_t *m);
double usage_model_topup_remaining(const usage_model_t *m);
double usage_model_total_spendable(const usage_model_t *m);
bool   usage_model_has_topup(const usage_model_t *m);
bool   usage_model_has_subscription_remaining(const usage_model_t *m);
bool   usage_model_has_topup_remaining(const usage_model_t *m);
bool   usage_model_has_total_spendable(const usage_model_t *m);
const usage_bar_t *usage_model_plan_bar(const usage_model_t *m);
const usage_bar_t *usage_model_topup_bar(const usage_model_t *m);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BILLING_USAGE_H */
