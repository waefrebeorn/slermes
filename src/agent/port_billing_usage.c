/* port_billing_usage.c — faithful C11 port of agent/billing_usage.py
 *
 * Pure-logic dollar usage model for the billing/subscription surfaces.
 * Reuses: account_usage_fmt_usd() (include/hermes_account_usage.h),
 *          libjson (json_t, json_get_*, json_dumps).
 * No IO/network: build_usage_model() and _dev_fixture_usage_model() are NOT
 * ported (they fetch account-info over the network) — they remain honest
 * REAL_GAPs. usage_model_from_account() consumes an already-parsed JSON
 * object, mirroring the real data flow.
 */

#define _XOPEN_SOURCE 700   /* strptime() (glibc X/Open extension) */

#include "billing_usage.h"
#include "hermes_account_usage.h"   /* account_usage_fmt_usd (reuse _fmt_usd) */
#include "hermes_json.h"             /* libjson */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#define LOW_BALANCE_THRESHOLD_USD 5.0

/* PoP: billing_usage__finite @ agent/billing_usage.py:_finite */
bool billing_usage_finite(const void *json_value, double *out) {
    const json_t *v = (const json_t *)json_value;
    if (!v || v->type != JSON_NUMBER) { *out = 0.0; return false; }
    double f = v->num_val;
    if (!isfinite(f)) { *out = 0.0; return false; }
    *out = f;
    return true;
}

/* PoP: billing_usage__fmt_usd @ agent/billing_usage.py:_fmt_usd */
void billing_usage_fmt_usd(double value, char *buf, size_t sz) {
    account_usage_fmt_usd(value, buf, sz);
}

/* PoP: billing_usage_format_renews @ agent/billing_usage.py:format_renews */
char *billing_usage_format_renews(const char *value) {
    if (!value) return NULL;
    char *text = strdup(value);
    if (!text) return NULL;
    /* strip surrounding whitespace */
    char *s = text;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t L = strlen(s);
    while (L > 0 && isspace((unsigned char)s[L-1])) s[--L] = '\0';
    if (*s == '\0') { free(text); return NULL; }

    /* ISO normalize: 'Z' suffix -> '+00:00' so fromisoformat-like parse works. */
    char iso[64];
    snprintf(iso, sizeof(iso), "%s", s);
    size_t il = strlen(iso);
    if (il > 0 && iso[il-1] == 'Z') {
        iso[il-1] = '+';
        if (il + 5 < sizeof(iso)) {
            iso[il] = '0'; iso[il+1] = '0'; iso[il+2] = ':';
            iso[il+3] = '0'; iso[il+4] = '0'; iso[il+5] = '\0';
        }
    }

    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    char *out = NULL;
    /* Try full ISO parse. */
    if (strptime(iso, "%Y-%m-%dT%H:%M:%S%z", &tm) ||
        strptime(iso, "%Y-%m-%dT%H:%M:%S", &tm) ||
        strptime(iso, "%Y-%m-%d %H:%M:%S", &tm)) {
        char mon[16];
        strftime(mon, sizeof(mon), "%b", &tm);
        /* day without leading zero (%-d isn't portable) */
        int day = tm.tm_mday;
        size_t need = strlen(mon) + 32;
        out = malloc(need);
        if (out) snprintf(out, need, "%s %d, %d", mon, day, tm.tm_year + 1900);
    } else if (L >= 10 && strptime(s, "%Y-%m-%d", &tm)) {
        char mon[16];
        strftime(mon, sizeof(mon), "%b", &tm);
        int day = tm.tm_mday;
        size_t need = strlen(mon) + 32;
        out = malloc(need);
        if (out) snprintf(out, need, "%s %d, %d", mon, day, tm.tm_year + 1900);
    } else {
        /* Unparsable -> return the raw text unchanged. */
        out = strdup(s);
    }
    free(text);
    return out ? out : strdup("");
}

/* ===================== UsageBar ===================== */
struct usage_bar {
    char  *kind;
    double remaining_usd;
    double total_usd;
    double spent_usd;
};

/* PoP: billing_usage_usagebar @ agent/billing_usage.py:UsageBar */
usage_bar_t *usage_bar_make(const char *kind, double remaining, double total,
                            double spent) {
    usage_bar_t *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->kind = strdup(kind ? kind : "");
    b->remaining_usd = remaining;
    b->total_usd = total;
    b->spent_usd = spent;
    return b;
}

void usage_bar_free(usage_bar_t *b) {
    if (!b) return;
    free(b->kind);
    free(b);
}

const char *usage_bar_kind(const usage_bar_t *b)    { return b ? b->kind : ""; }
double      usage_bar_remaining(const usage_bar_t *b){ return b ? b->remaining_usd : 0.0; }
double      usage_bar_total(const usage_bar_t *b)   { return b ? b->total_usd : 0.0; }
double      usage_bar_spent(const usage_bar_t *b)   { return b ? b->spent_usd : 0.0; }

/* PoP: billing_usage_usagebar_pct_used @ agent/billing_usage.py:UsageBar.pct_used */
bool usage_bar_pct_used(const usage_bar_t *b, int *out_pct) {
    if (!b || strcmp(b->kind, "plan") != 0 || b->total_usd <= 0) return false;
    int pct = (int)round(b->spent_usd / b->total_usd * 100.0);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    *out_pct = pct;
    return true;
}

/* PoP: billing_usage_usagebar_fill_fraction @ agent/billing_usage.py:UsageBar.fill_fraction */
double usage_bar_fill_fraction(const usage_bar_t *b) {
    if (!b || b->total_usd <= 0) return 0.0;
    double f = b->remaining_usd / b->total_usd;
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    return f;
}

/* ===================== UsageModel ===================== */
struct usage_model {
    bool   available;
    char  *status;
    char  *plan_name;
    char  *renews_at;
    char  *renews_display;
    double subscription_remaining_usd;
    double topup_remaining_usd;
    double total_spendable_usd;
    bool subscription_remaining_present;
    bool topup_remaining_present;
    bool total_spendable_present;
    usage_bar_t *plan_bar;
    usage_bar_t *topup_bar;
};

static usage_model_t *usage_model_make_available_false(void) {
    usage_model_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->available = false;
    m->status = strdup("free");
    /* All numeric fields are absent (Python's dataclass defaults -> None). */
    m->subscription_remaining_present = false;
    m->topup_remaining_present = false;
    m->total_spendable_present = false;
    return m;
}

/* PoP: billing_usage_usage_model_from_account @ agent/billing_usage.py:usage_model_from_account */
usage_model_t *usage_model_from_account(const void *account_info_json) {
    const json_t *ai = (const json_t *)account_info_json;
    if (!ai || ai->type != JSON_OBJECT) return usage_model_make_available_false();

    bool logged_in = json_get_bool(ai, "logged_in", false);
    if (!logged_in) return usage_model_make_available_false();

    /* paid_service_access_info */
    json_t *access = json_obj_get(ai, "paid_service_access_info");
    /* subscription */
    json_t *sub = json_obj_get(ai, "subscription");
    /* paid_service_access: Python uses `paid is False` (identity, not
     * truthiness) -> only a literal JSON false triggers "depleted";
     * null/absent (None) does NOT. */
    json_t *paid_node = json_obj_get(ai, "paid_service_access");
    bool paid_is_false = (paid_node && paid_node->type == JSON_BOOL && paid_node->bool_val == false);

    double sub_remaining = 0, topup_remaining = 0, total_usable = 0;
    bool has_sub_rem = billing_usage_finite(json_obj_get(access, "subscription_credits_remaining"), &sub_remaining);
    bool has_topup    = billing_usage_finite(json_obj_get(access, "purchased_credits_remaining"), &topup_remaining);
    bool has_total    = billing_usage_finite(json_obj_get(access, "total_usable_credits"), &total_usable);

    /* None-valued strings are stored as NULL to mirror Python's None -> JSON null. */
    const char *plan_name = json_get_str(sub, "plan", NULL);
    const char *renews_at = json_get_str(sub, "current_period_end", NULL);
    double monthly = 0;
    bool has_monthly = billing_usage_finite(json_obj_get(sub, "monthly_credits"), &monthly);

    bool has_subscription = (plan_name && *plan_name) || (has_monthly && monthly > 0);

    /* total spendable: prefer server total; else sum the parts. */
    double total_spendable;
    bool has_total_spendable;
    if (has_total) { total_spendable = total_usable; has_total_spendable = true; }
    else {
        double parts[2];
        int n = 0;
        if (has_sub_rem) parts[n++] = sub_remaining;
        if (has_topup)   parts[n++] = topup_remaining;
        if (n > 0) { total_spendable = 0; for (int i=0;i<n;i++) total_spendable += parts[i]; has_total_spendable = true; }
        else { has_total_spendable = false; }
    }

    /* status classification */
    const char *status;
    if (paid_is_false) status = "depleted";
    else if (!has_subscription && !(has_topup && topup_remaining > 0)) status = "free";
    else if (has_total_spendable && total_spendable < LOW_BALANCE_THRESHOLD_USD) status = "low";
    else status = "healthy";

    /* plan bar */
    usage_bar_t *plan_bar = NULL;
    if (has_monthly && monthly > 0 && has_sub_rem) {
        double remaining = sub_remaining;
        if (remaining < 0) remaining = 0;
        if (remaining > monthly) remaining = monthly;
        double spent = monthly - sub_remaining;
        if (spent < 0) spent = 0;
        plan_bar = usage_bar_make("plan", remaining, monthly, spent);
    }

    /* topup bar */
    usage_bar_t *topup_bar = NULL;
    if (has_topup && topup_remaining > 0) {
        topup_bar = usage_bar_make("topup", topup_remaining, topup_remaining, 0.0);
    }

    char *renews_display = billing_usage_format_renews(renews_at);

    usage_model_t *m = calloc(1, sizeof(*m));
    if (!m) { usage_bar_free(plan_bar); usage_bar_free(topup_bar); free(renews_display); return NULL; }
    m->available = true;
    m->status = strdup(status);
    m->plan_name = plan_name ? strdup(plan_name) : NULL;   /* NULL -> JSON null (Python None) */
    m->renews_at = renews_at ? strdup(renews_at) : NULL;
    m->renews_display = renews_display;   /* already NULL when no renews_at */
    m->subscription_remaining_usd = has_sub_rem ? sub_remaining : 0.0;
    m->subscription_remaining_present = has_sub_rem;
    m->topup_remaining_usd = has_topup ? topup_remaining : 0.0;
    m->topup_remaining_present = has_topup;
    m->total_spendable_usd = has_total_spendable ? total_spendable : 0.0;
    m->total_spendable_present = has_total_spendable;
    m->plan_bar = plan_bar;
    m->topup_bar = topup_bar;
    return m;
}

void usage_model_free(usage_model_t *m) {
    if (!m) return;
    free(m->status);
    free(m->plan_name);
    free(m->renews_at);
    free(m->renews_display);
    usage_bar_free(m->plan_bar);
    usage_bar_free(m->topup_bar);
    free(m);
}

bool   usage_model_available(const usage_model_t *m)  { return m ? m->available : false; }
const char *usage_model_status(const usage_model_t *m){ return m ? m->status : "free"; }
const char *usage_model_plan_name(const usage_model_t *m){ return m ? m->plan_name : ""; }
const char *usage_model_renews_at(const usage_model_t *m){ return m ? m->renews_at : ""; }
const char *usage_model_renews_display(const usage_model_t *m){ return m ? m->renews_display : ""; }
double usage_model_subscription_remaining(const usage_model_t *m){ return m ? m->subscription_remaining_usd : 0.0; }
double usage_model_topup_remaining(const usage_model_t *m){ return m ? m->topup_remaining_usd : 0.0; }
double usage_model_total_spendable(const usage_model_t *m){ return m ? m->total_spendable_usd : 0.0; }

/* PoP: billing_usage_usagemodel_has_topup @ agent/billing_usage.py:UsageModel.has_topup */
bool usage_model_has_topup(const usage_model_t *m) {
    return m && m->topup_remaining_usd > 0;
}

bool usage_model_has_subscription_remaining(const usage_model_t *m) {
    return m && m->subscription_remaining_present;
}
bool usage_model_has_topup_remaining(const usage_model_t *m) {
    return m && m->topup_remaining_present;
}
bool usage_model_has_total_spendable(const usage_model_t *m) {
    return m && m->total_spendable_present;
}

const usage_bar_t *usage_model_plan_bar(const usage_model_t *m)  { return m ? m->plan_bar : NULL; }
const usage_bar_t *usage_model_topup_bar(const usage_model_t *m) { return m ? m->topup_bar : NULL; }
