/**
 * port_agent_billing_view.c — Port of Python agent/billing_view.py
 *
 * Real C implementations for billing state parsing and HTTP fetch.
 * Uses popen(curl ...) for HTTP — keeps it self-contained.
 */

#ifndef SRC_AGENT_PORT_AGENT_BILLING_VIEW_C
#define SRC_AGENT_PORT_AGENT_BILLING_VIEW_C

#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_billing.h"
#include "libhttp/http.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

/* PoP: parse_money @ agent/billing_view.py:parse_money */
/* Port of Python agent/billing_view.py:parse_money */

/* PoP: format_money @ agent/billing_view.py:format_money */
/* Port of Python agent/billing_view.py:format_money */

/* PoP: masked @ agent/billing_view.py:CardInfo.masked */
/* Port of Python agent/billing_view.py:CardInfo.masked */

/* PoP: can_charge @ agent/billing_view.py:BillingState.can_charge */
/* Port of Python agent/billing_view.py:BillingState.can_charge */

/* PoP: _parse_card @ agent/billing_view.py:_parse_card */
/* Port of Python agent/billing_view.py:_parse_card */

/* PoP: _parse_monthly_cap @ agent/billing_view.py:_parse_monthly_cap */
/* Port of Python agent/billing_view.py:_parse_monthly_cap */

/* PoP: _parse_auto_reload @ agent/billing_view.py:_parse_auto_reload */
/* Port of Python agent/billing_view.py:_parse_auto_reload */

/* PoP: billing_state_from_payload @ agent/billing_view.py:billing_state_from_payload */
/* Port of Python agent/billing_view.py:billing_state_from_payload */

/* PoP: build_billing_state @ agent/billing_view.py:build_billing_state */
/* Port of Python agent/billing_view.py:build_billing_state */

/* PoP: _fallback_portal_url @ agent/billing_view.py:_fallback_portal_url */
/* Port of Python agent/billing_view.py:_fallback_portal_url */

/* PoP: new_idempotency_key @ agent/billing_view.py:new_idempotency_key */
/* Port of Python agent/billing_view.py:new_idempotency_key */

/* PoP: validate_charge_amount @ agent/billing_view.py:validate_charge_amount */
/* Port of Python agent/billing_view.py:validate_charge_amount */

/* ================================================================
 *  Internal: HTTP fetch via curl popen
 * ================================================================ */

/* fetch_billing_page renamed to avoid shadowing lib/libhttp/http.h:http_get().
 * Uses the internal libhttp client — no external curl binary. */
static char *billing_fetch(const char *url, const char *auth_header) {
    if (!url) return NULL;

    http_t *h = http_new(30);
    if (!h) return strdup("{\"error\":\"http client init failed\"}");

    char headers[512];
    if (auth_header && *auth_header)
        snprintf(headers, sizeof(headers), "Authorization: %s\r\nAccept: application/json", auth_header);
    else
        snprintf(headers, sizeof(headers), "Accept: application/json");

    http_resp_t *r = http_get(h, url, headers);
    http_free(h);
    if (!r) return strdup("{\"error\":\"http request failed\"}");

    char *out = NULL;
    if (r->body && r->body[0]) {
        out = strdup(r->body);
    } else {
        out = strdup("{\"error\":\"empty response\"}");
    }
    http_resp_free(r);
    return out;
}

/* ================================================================
 *  JSON helpers (manual parsing for simple types)
 * ================================================================ */

/* billing_json helpers live in billing_json_helpers.c (hermes_billing.h) */

static char *json_get_string(const char *json, const char *key, char *buf, size_t buf_sz) {
/* PoP: parse_budget_ceiling @ agent/billing_view.py:parse_budget_ceiling */
    if (!json || !key || !buf || buf_sz == 0) return NULL;
    buf[0] = '\0';
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p && *p != ':') p++;
    if (*p) p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    if (len >= buf_sz) len = buf_sz - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

/* ================================================================
 *  Data structures: provided by hermes_billing.h (avoid duplicate
 *  definitions that conflict with the header's typedefs).
 * ================================================================ */

/* ================================================================
 *  Parsing functions
 * ================================================================ */

static billing_card_t parse_card(const char *json) {
    billing_card_t card = {0};
    if (!json) return card;

    char brand[64], last4[8];
    if (json_get_string(json, "brand", brand, sizeof(brand)) &&
        json_get_string(json, "last4", last4, sizeof(last4))) {
        strncpy(card.brand, brand, sizeof(card.brand) - 1);
        strncpy(card.last4, last4, sizeof(card.last4) - 1);
        card.valid = true;
    }
    return card;
}

static billing_monthly_cap_t parse_monthly_cap(const char *json) {
    billing_monthly_cap_t cap = {0};
    if (!json) return cap;

    double limit = billing_json_get_number(json, "limitUsd");
    double spent = billing_json_get_number(json, "spentThisMonthUsd");
    bool is_default = billing_json_get_bool(json, "isDefaultCeiling");

    if (limit > 0 || spent > 0) {
        cap.limit_usd = limit;
        cap.spent_this_month_usd = spent;
        cap.is_default_ceiling = is_default;
        cap.has_value = true;
    }
    return cap;
}

static billing_auto_reload_t parse_auto_reload(const char *json) {
    billing_auto_reload_t ar = {0};
    if (!json) return ar;

    bool enabled = billing_json_get_bool(json, "enabled");
    double threshold = billing_json_get_number(json, "thresholdUsd");
    double reload_to = billing_json_get_number(json, "reloadToUsd");

    if (enabled || threshold > 0 || reload_to > 0) {
        ar.enabled = enabled;
        ar.threshold_usd = threshold;
        ar.reload_to_usd = reload_to;
        ar.has_value = true;
    }
    return ar;
}

/* ================================================================
 *  Public API: billing_state_from_payload
 * ================================================================ */

billing_state_t billing_state_from_payload(const char *payload_json, const char *portal_url) {
    billing_state_t state = {0};
    if (!payload_json) return state;

    state.logged_in = true;

    json_get_string(payload_json, "orgId", state.org_id, sizeof(state.org_id));
    json_get_string(payload_json, "orgSlug", state.org_slug, sizeof(state.org_slug));
    json_get_string(payload_json, "orgName", state.org_name, sizeof(state.org_name));
    json_get_string(payload_json, "role", state.role, sizeof(state.role));
    state.balance_usd = billing_json_get_number(payload_json, "balanceUsd");
    state.cli_billing_enabled = billing_json_get_bool(payload_json, "cliBillingEnabled");

    /* Parse charge presets array */
    const char *presets = strstr(payload_json, "\"chargePresets\"");
    if (presets) {
        presets = strchr(presets, '[');
        if (presets) {
            presets++;
            while (*presets && state.charge_preset_count < 16) {
                while (*presets && (*presets == ' ' || *presets == ',' || *presets == '\n' || *presets == '\t')) presets++;
                if (!*presets || *presets == ']') break;
                state.charge_presets[state.charge_preset_count++] = atof(presets);
                while (*presets && *presets != ',' && *presets != ']') presets++;
                if (*presets == ',') presets++;
            }
        }
    }

    /* Bounds */
    const char *bounds = strstr(payload_json, "\"bounds\"");
    if (bounds) {
        bounds = strchr(bounds, '{');
        if (bounds) {
            state.min_usd = billing_json_get_number(bounds, "minUsd");
            state.max_usd = billing_json_get_number(bounds, "maxUsd");
        }
    }

    /* Card */
    const char *card_obj = strstr(payload_json, "\"card\"");
    if (card_obj) {
        card_obj = strchr(card_obj, '{');
        if (card_obj) {
            state.card = parse_card(card_obj);
        }
    }

    /* Monthly cap */
    const char *cap_obj = strstr(payload_json, "\"monthlyCap\"");
    if (cap_obj) {
        cap_obj = strchr(cap_obj, '{');
        if (cap_obj) {
            state.monthly_cap = parse_monthly_cap(cap_obj);
        }
    }

    /* Auto-reload */
    const char *ar_obj = strstr(payload_json, "\"autoReload\"");
    if (ar_obj) {
        ar_obj = strchr(ar_obj, '{');
        if (ar_obj) {
            state.auto_reload = parse_auto_reload(ar_obj);
        }
    }

    /* Portal URL */
    if (portal_url) {
        strncpy(state.portal_url, portal_url, sizeof(state.portal_url) - 1);
    }

    return state;
}

/* ================================================================
 *  Public API: build_billing_state (fetch from server)
 * ================================================================ */

billing_state_t build_billing_state(void) {
    billing_state_t state = {0};

    /* Get API key from environment */
    const char *api_key = getenv("NOUS_BILLING_KEY");
    if (!api_key || !*api_key) {
        api_key = getenv("NOUS_API_KEY");
    }
    if (!api_key || !*api_key) {
        state.logged_in = false;
        strncpy(state.error, "NOUS_BILLING_KEY not set", sizeof(state.error) - 1);
        return state;
    }

    /* Resolve portal base URL */
    char portal_base[512];
    const char *env_url = getenv("NOUS_PORTAL_URL");
    if (env_url && *env_url) {
        strncpy(portal_base, env_url, sizeof(portal_base) - 1);
    } else {
        strcpy(portal_base, "https://billing.nousresearch.com");
    }

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/billing/state", portal_base);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    char *resp = billing_fetch(url, auth_header);
    if (!resp) {
        state.logged_in = false;
        strncpy(state.error, "HTTP request failed", sizeof(state.error) - 1);
        return state;
    }

    /* Check for auth error */
    if (strstr(resp, "401") || strstr(resp, "403") || strstr(resp, "\"error\"")) {
        state.logged_in = false;
        if (strstr(resp, "401") || strstr(resp, "403")) {
            strncpy(state.error, "Not authenticated", sizeof(state.error) - 1);
        } else {
            strncpy(state.error, resp, sizeof(state.error) - 1);
        }
        free(resp);
        return state;
    }

    /* Try to get portal URL from response */
    char portal_url[512];
    bool has_portal = json_get_string(resp, "portalUrl", portal_url, sizeof(portal_url));

    billing_state_t parsed = billing_state_from_payload(resp, has_portal ? portal_url : NULL);
    free(resp);

    /* Build fallback portal URL if not provided */
    if (!parsed.portal_url[0]) {
        snprintf(parsed.portal_url, sizeof(parsed.portal_url), "%s/billing?topup=open", portal_base);
    }

    return parsed;
}

/* ================================================================
 *  Public API: can_charge
 * ================================================================ */

bool billing_can_charge(const billing_state_t *state) {
    if (!state || !state->logged_in) return false;
    /* Admin role AND kill-switch on */
    if (strcasecmp(state->role, "OWNER") == 0 || strcasecmp(state->role, "ADMIN") == 0) {
        return state->cli_billing_enabled;
    }
    return false;
}

/* ================================================================
 *  Public API: format_money
 * ================================================================ */

void billing_format_money(double amount, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    if (amount == (long long)amount) {
        snprintf(out, out_sz, "$%.0f", amount);
    } else {
        snprintf(out, out_sz, "$%.2f", amount);
    }
}

/* ================================================================
 *  Public API: masked
 * ================================================================ */

void billing_masked(const billing_card_t *card, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    if (!card || !card->valid) {
        strncpy(out, "N/A", out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }
    snprintf(out, out_sz, "%s ****%s", card->brand, card->last4);
}

/* ================================================================
 *  Public API: new_idempotency_key
 * ================================================================ */

void billing_new_idempotency_key(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    /* Generate UUID v4 style */
    unsigned int r1 = rand();
    unsigned int r2 = rand();
    unsigned int r3 = rand();
    unsigned int r4 = rand();
    snprintf(out, out_sz, "%08x-%04x-%04x-%04x-%04x%08x",
             r1, r2 & 0xffff, r3 & 0xffff, r4 & 0xffff,
             (r1 >> 16) & 0xffff, r2);
}

/* ================================================================
 *  Public API: parse_money
 * ================================================================ */

double billing_parse_money(const char *str) {
    if (!str) return 0.0;
    while (*str == '$' || *str == ' ') str++;
    return atof(str);
}

/* ================================================================
 *  Public API: validate_charge_amount
 * ================================================================ */

bool billing_validate_charge_amount(double amount, double min_usd, double max_usd) {
    if (amount <= 0.0) return false;
    if (min_usd > 0 && amount < min_usd) return false;
    if (max_usd > 0 && amount > max_usd) return false;
    /* Check 2 decimal places max */
    double rounded = (double)((long long)(amount * 100.0 + 0.5)) / 100.0;
    if (fabs(amount - rounded) > 0.0001) return false;
    return true;
}

/* ================================================================
 *  Portal URL helpers
 * ================================================================ */

const char *billing_fallback_portal_url(void) {
    hermes_log(LOG_DEBUG, "port", "billing_fallback_portal_url: returning default");
    return "https://billing.nousresearch.com/portal";
}

#endif /* SRC_AGENT_PORT_AGENT_BILLING_VIEW_C */