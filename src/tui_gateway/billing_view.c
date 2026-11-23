/*
 * billing_view.c - faithful C11 port of selected pure serializers from
 * upstream Hermes tui_gateway/billing_view.py (gate w11_billingvie_7423e1).
 *
 * PoP: w11_billingvie_7423e1_wire_str @ tui_gateway/billing_view.py:_wire_str
 * PoP: w11_billingvie_7423e1_billing_pick @ tui_gateway/billing_view.py:_billing_pick
 * PoP: w11_billingvie_7423e1_payment_method @ tui_gateway/billing_view.py:_serialize_payment_method
 * PoP: w11_billingvie_7423e1_subscription_preview @ tui_gateway/billing_view.py:_serialize_subscription_preview
 * PoP: w11_billingvie_7423e1_auto_reload @ tui_gateway/billing_view.py:_serialize_auto_reload
 *
 * State-bound RPC registration, error envelopes and the account-info builders are
 * intentionally not ported (process-bound / external services).
 */
#include "noc_w11_billingvie_7423e1.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python dict.get(src) yields None for a missing key: keep the key with a JSON null. */
static json_t *clone_or_null(const json_t *obj, const char *key) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return v ? json_copy(v) : json_null();
}

/* Python str(value) for the wire scalars used by these serializers. */
static char *wire_text(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        snprintf(buf, sizeof(buf), "%.15g", x);
        if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E')) {
            size_t n = strlen(buf);
            if (n + 2 < sizeof(buf)) { buf[n] = '.'; buf[n + 1] = '0'; buf[n + 2] = '\0'; }
        }
        return strdup(buf);
    }
    return strdup(""); /* containers are outside these serializers' contract */
}

json_t *w11_billingvie_7423e1_wire_str(const json_t *value) {
    char *s = wire_text(value);
    if (!s) return json_null();
    json_t *r = json_string(s);
    free(s);
    return r;
}

json_t *w11_billingvie_7423e1_billing_pick(const json_t *result, const json_t *fields) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    if (!json_is_object(fields)) return out;
    for (size_t i = 0; i < json_object_size(fields); i++) {
        const char *k = json_object_get_key_at(fields, i);
        json_t *srcv = json_object_get_at(fields, i);
        const char *src = (srcv && srcv->type == JSON_STRING) ? srcv->str_val : "";
        json_t *v = json_obj_get(result, src);
        json_set(out, k, v ? json_copy(v) : json_null());
    }
    return out;
}

/* Mirrors agent.billing_view.format_money for the Decimal-shaped fixture strings. */
static char *format_money_text(const char *s) {
    if (!s) return strdup("\xE2\x80\x94"); /* em dash */
    double x = atof(s);
    char buf[128];
    if (x == floor(x)) snprintf(buf, sizeof(buf), "$%.0f", x);
    else snprintf(buf, sizeof(buf), "$%.2f", x);
    return strdup(buf);
}

static void set_wire_and_display(json_t *obj, const char *wire_key, const char *disp_key,
                                 const json_t *value) {
    json_t *w = w11_billingvie_7423e1_wire_str(value);
    json_set(obj, wire_key, w);
    const char *raw = (value && value->type == JSON_STRING) ? value->str_val : NULL;
    char *d = format_money_text(raw);
    json_set(obj, disp_key, json_string(d ? d : ""));
    free(d);
}

json_t *w11_billingvie_7423e1_payment_method(const json_t *pm) {
    if (!pm || pm->type == JSON_NULL) return json_null();
    const char *kind = json_get_str(pm, "kind", "");
    json_t *out = json_object();
    if (!strcmp(kind, "card")) {
        json_set(out, "kind", json_string("card"));
        json_set(out, "brand", clone_or_null(pm, "brand"));
        json_set(out, "last4", clone_or_null(pm, "last4"));
        json_set(out, "wallet", clone_or_null(pm, "wallet"));
        json_set(out, "resolved_via", clone_or_null(pm, "resolved_via"));
    } else if (!strcmp(kind, "link")) {
        json_set(out, "kind", json_string("link"));
        json_set(out, "email", clone_or_null(pm, "email"));
        json_set(out, "resolved_via", clone_or_null(pm, "resolved_via"));
    } else {
        json_set(out, "kind", json_string("unknown"));
        json_set(out, "raw_kind", clone_or_null(pm, "raw_kind"));
        json_set(out, "resolved_via", clone_or_null(pm, "resolved_via"));
    }
    return out;
}

json_t *w11_billingvie_7423e1_subscription_preview(const json_t *p) {
    if (!p || p->type == JSON_NULL) return json_null();
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    const char *keys[] = {"effect", "reason", "current_tier_id", "current_tier_name",
                          "target_tier_id", "target_tier_name", "effective_at"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        json_set(out, keys[i], clone_or_null(p, keys[i]));
    json_set(out, "monthly_credits_delta",
             w11_billingvie_7423e1_wire_str(json_obj_get(p, "monthly_credits_delta")));
    json_set(out, "amount_due_now_cents", clone_or_null(p, "amount_due_now_cents"));
    return out;
}

json_t *w11_billingvie_7423e1_auto_reload(const json_t *ar) {
    if (!ar || ar->type == JSON_NULL) return json_null();
    json_t *out = json_object();
    json_t *enabled = json_obj_get(ar, "enabled");
    json_set(out, "enabled", enabled ? json_copy(enabled) : json_null());
    set_wire_and_display(out, "threshold_usd", "threshold_display", json_obj_get(ar, "threshold_usd"));
    set_wire_and_display(out, "reload_to_usd", "reload_to_display", json_obj_get(ar, "reload_to_usd"));
    json_t *card = json_obj_get(ar, "card");
    if (!card || card->type == JSON_NULL) {
        json_set(out, "card", json_null());
    } else {
        json_t *co = json_object();
        const char *kind = json_get_str(card, "kind", "");
        json_set(co, "kind", json_string(kind));
        if (!strcmp(kind, "distinct")) {
            json_set(co, "payment_method_id", clone_or_null(card, "payment_method_id"));
            json_set(co, "brand", clone_or_null(card, "brand"));
            json_set(co, "last4", clone_or_null(card, "last4"));
        }
        json_set(out, "card", co);
    }
    return out;
}
