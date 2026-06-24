/*
 * port_agent_billing_view.c — Port of Python agent/billing_view.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"


/* Port of Python: _fallback_portal_url */
const char *billing_fallback_portal_url(void) {
    hermes_log(LOG_DEBUG, "port", "billing_fallback_portal_url: returning default");
    return "https://billing.nousresearch.com/portal";
}


/* Port of Python: _parse_auto_reload */
bool billing_parse_auto_reload(const char *json) {
    if (!json) return false;
    bool has_auto_reload = (strstr(json, "\"auto_reload\"") != NULL);
    bool is_true = (strstr(json, "\"true\"") != NULL);
    return has_auto_reload && is_true;
}


/* Port of Python: _parse_card */
typedef struct {
    char last4[5];
    char brand[32];
    char exp_month[3];
    char exp_year[5];
    bool valid;
} billing_card_t;

billing_card_t billing_parse_card(const char *json) {
    billing_card_t card = {0};
    if (!json) return card;
    
    const char *last4 = strstr(json, "\"last4\"");
    if (last4) {
        const char *val = strchr(last4 + 7, '"');
        if (val) {
            val++;
            for (int i = 0; i < 4 && val[i] && val[i] != '"'; i++)
                card.last4[i] = val[i];
            card.last4[4] = '\0';
        }
    }
    
    const char *brand = strstr(json, "\"brand\"");
    if (brand) {
        const char *val = strchr(brand + 7, '"');
        if (val) {
            val++;
            for (int i = 0; i < 31 && val[i] && val[i] != '"'; i++)
                card.brand[i] = val[i];
            card.brand[31] = '\0';
        }
    }
    
    card.valid = (card.last4[0] != '\0');
    return card;
}


/* Port of Python: _parse_monthly_cap */
double billing_parse_monthly_cap(const char *json) {
    if (!json) return 0.0;
    const char *cap = strstr(json, "\"monthly_cap\"");
    if (!cap) return 0.0;
    const char *val = strchr(cap + 13, ':');
    if (!val) return 0.0;
    val++;
    while (*val == ' ') val++;
    double result = atof(val);
    return result;
}


/* Port of Python: billing_state_from_payload */
typedef struct {
    double balance;
    double monthly_cap;
    bool auto_reload;
    bool paid_access;
    billing_card_t card;
    bool valid;
} billing_state_t;

billing_state_t billing_state_from_payload(const char *json) {
    billing_state_t state = {0};
    if (!json) return state;
    
    state.auto_reload = billing_parse_auto_reload(json);
    state.monthly_cap = billing_parse_monthly_cap(json);
    state.card = billing_parse_card(json);
    
    /* Parse balance */
    const char *bal = strstr(json, "\"balance\"");
    if (bal) {
        const char *val = strchr(bal + 9, ':');
        if (val) {
            val++;
            while (*val == ' ') val++;
            state.balance = atof(val);
        }
    }
    
    /* Parse paid_access */
    const char *paid = strstr(json, "\"paid_access\"");
    if (paid) {
        state.paid_access = (strstr(paid, "true") != NULL);
    }
    
    state.valid = true;
    return state;
}


/* Port of Python: build_billing_state */
billing_state_t build_billing_state(const char *api_response) {
    return billing_state_from_payload(api_response);
}


/* Port of Python: can_charge */
bool billing_can_charge(const billing_state_t *state) {
    if (!state || !state->valid) return false;
    bool ok = state->paid_access;
    if (ok) return true;
    return false;
}


/* Port of Python: format_money */
void billing_format_money(double amount, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    snprintf(out, out_sz, "$%.2f", amount);
}


/* Port of Python: masked */
void billing_masked(const billing_card_t *card, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    if (!card || !card->valid) {
        strncpy(out, "N/A", out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }
    snprintf(out, out_sz, "%s ****%s", card->brand, card->last4);
}


/* Port of Python: new_idempotency_key */
void billing_new_idempotency_key(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    /* Generate a simple UUID-like key */
    snprintf(out, out_sz, "%08x-%04x-%04x-%04x-%012x",
             rand(), rand() & 0xffff, rand() & 0xffff,
             rand() & 0xffff, (unsigned long)time(NULL));
}


/* Port of Python: parse_money */
double billing_parse_money(const char *str) {
    if (!str) return 0.0;
    /* Remove $ and parse */
    while (*str == '$' || *str == ' ') str++;
    return atof(str);
}


/* Port of Python: validate_charge_amount */
bool billing_validate_charge_amount(double amount) {
    bool positive = (amount > 0.0);
    bool within_limit = (amount <= 10000.0);
    return positive && within_limit;
}

