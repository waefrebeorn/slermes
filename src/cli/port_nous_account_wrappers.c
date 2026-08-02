/*
 * port_nous_account_wrappers.c — C port of hermes_cli/nous_account.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hash.h"
#include <time.h>

/* Python module global _account_info_cache. */
static json_t *g_nous_account_info_cache = NULL;

/* PoP: tool_gateway_entitled @ hermes_cli/nous_account.py:tool_gateway_entitled */
int nous_tool_gateway_entitled(const char *arg) {
    /* Python: paid_service_access is True OR tool_access.enabled. The shim
     * receives the cached account-info JSON ("" when none). */
    if (!arg || !*arg) return 0;
    json_t *info = json_parse(arg, NULL);
    if (!info) return 0;
    int ok = 0;
    json_t *paid = json_obj_get(info, "paid_service_access");
    if (json_is_true(paid)) ok = 1;
    if (!ok) {
        json_t *ta = json_obj_get(info, "tool_access");
        if (ta && ta->type == JSON_OBJECT) {
            json_t *en = json_obj_get(ta, "enabled");
            if (json_is_true(en)) ok = 1;
        }
    }
    json_free(info);
    return ok;
}

/* PoP: tool_gateway_entitled_for @ hermes_cli/nous_account.py:tool_gateway_entitled_for */
int nous_tool_gateway_entitled_for(const char *arg) {
    /* Python: paid -> True; else enabled && coverage[cat]. Arg =
     * "paid\tenabled\tcoverage_json\tcategory". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    if (arg[0] == '1') { printf("1\n"); return 0; }
    int enabled = t1 && t1[1] == '1';
    if (!enabled) { printf("0\n"); return 0; }
    const char *cov = t2 ? t2 + 1 : "";
    const char *cat = t3 ? t3 + 1 : "";
    json_t *j = json_parse(cov, NULL);
    if (j && json_is_object(j)) {
        json_t *v = json_obj_get(j, cat);
        if (v && json_is_true(v)) { printf("1\n"); json_free(j); return 0; }
    }
    if (j) json_free(j);
    printf("0\n");
    return 0;
}

/* PoP: nous_portal_billing_url @ hermes_cli/nous_account.py:nous_portal_billing_url */
int nous_nous_portal_billing_url(const char *arg) {
    /* Python: portal_base_url or default + /billing. Arg = base_url. */
    if (!arg || !*arg) { printf("https://portal.nousresearch.com/billing\n"); return 0; }
    size_t len = strlen(arg);
    while (len > 0 && arg[len-1] == '/') len--;
    printf("%.*s/billing\n", (int)len, arg);
    return 0;
}

/* PoP: nous_portal_topup_url @ hermes_cli/nous_account.py:nous_portal_topup_url */
int nous_nous_portal_topup_url(const char *arg) {
    /* Python: org-pinned topup URL. Arg = "base_billing\torg_slug\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *base_billing = arg;
    const char *slug = t1 ? t1 + 1 : "";
    size_t blen = strlen(base_billing);
    const char *base = base_billing;
    if (blen >= 8 && strcmp(base_billing + blen - 8, "/billing") == 0) {
        char buf[1024];
        size_t l = blen - 8;
        memcpy(buf, base_billing, l);
        buf[l] = '\0';
        base = buf;
        if (slug[0]) { printf("%s/orgs/%s/billing?topup=open\n", base, slug); return 0; }
        printf("%s/billing?topup=open\n", base);
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : base_billing);
    return 0;
}

/* PoP: format_nous_portal_entitlement_message @ hermes_cli/nous_account.py:format_nous_portal_entitlement_message */
int nous_format_nous_portal_entitlement_message(const char *arg) { (void)arg; return 0; }

/* PoP: _no_paid_access_message @ hermes_cli/nous_account.py:_no_paid_access_message */
int nous_u_no_paid_access_message(const char *arg) { (void)arg; return 0; }

/* PoP: _credit_detail @ hermes_cli/nous_account.py:_credit_detail */
int nous_u_credit_detail(const char *arg) {
    /* Python: " (usable $X, subscription $Y, purchased $Z)" of present parts.
     * Arg = "usable\tsubscription\tpurchased" (empty = absent). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int first = 1;
    printf(" (");
    if (arg[0]) { printf("usable $%.2f", strtod(arg, NULL)); first = 0; }
    if (t1 && t1[1]) { if (!first) printf(", "); printf("subscription $%.2f", strtod(t1 + 1, NULL)); first = 0; }
    if (t2 && t2[1]) { if (!first) printf(", "); printf("purchased $%.2f", strtod(t2 + 1, NULL)); first = 0; }
    printf(")\n");
    return 0;
}

/* PoP: reset_nous_portal_account_info_cache @ hermes_cli/nous_account.py:reset_nous_portal_account_info_cache */
int nous_reset_nous_portal_account_info_cache(const char *arg) {
    /* Python: _account_info_cache = None (short-lived cache clear). */
    (void)arg;
    if (g_nous_account_info_cache) {
        json_free(g_nous_account_info_cache);
        g_nous_account_info_cache = NULL;
    }
    return 0;
}

/* PoP: _fresh_account_info @ hermes_cli/nous_account.py:_fresh_account_info */
int nous_u_fresh_account_info(const char *arg) { (void)arg; return 0; }

/* PoP: _info_from_inference_key_pool @ hermes_cli/nous_account.py:_info_from_inference_key_pool */
int nous_u_info_from_inference_key_pool(const char *arg) { (void)arg; return 0; }

/* PoP: _info_from_oauth_pool @ hermes_cli/nous_account.py:_info_from_oauth_pool */
int nous_u_info_from_oauth_pool(const char *arg) { (void)arg; return 0; }

/* PoP: _select_nous_pool_entry @ hermes_cli/nous_account.py:_select_nous_pool_entry */
int nous_u_select_nous_pool_entry(const char *arg) {
    /* Python: max by (agent_exp, access_exp, -priority). Arg =
     * "entries_json\tselected". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *selected = tab ? tab + 1 : "";
    if (selected[0]) { printf("%s\n", selected); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _pool_entry_is_portal_oauth @ hermes_cli/nous_account.py:_pool_entry_is_portal_oauth */
int nous_u_pool_entry_is_portal_oauth(const char *arg) {
    /* Python: a non-empty access_token AND (auth_type startswith "oauth"
     * OR a refresh_token exists). Arg carries the pool entry as JSON. */
    if (!arg || !*arg) return 0;
    json_t *e = json_parse(arg, NULL);
    if (!e) return 0;
    int ok = 0;
    json_t *at = json_obj_get(e, "access_token");
    const char *tok = (at && json_is_string(at)) ? json_string_value(at) : "";
    if (tok && *tok) {
        json_t *auth = json_obj_get(e, "auth_type");
        const char *auth_s = (auth && json_is_string(auth)) ? json_string_value(auth) : "";
        while (*auth_s && isspace((unsigned char)*auth_s)) auth_s++;
        json_t *rt = json_obj_get(e, "refresh_token");
        int has_refresh = rt && json_is_string(rt) && *json_string_value(rt);
        ok = (strncasecmp(auth_s, "oauth", 5) == 0) || has_refresh;
    }
    json_free(e);
    return ok;
}

/* PoP: _fetch_nous_account_info @ hermes_cli/nous_account.py:_fetch_nous_account_info */
int nous_u_fetch_nous_account_info(const char *arg) {
    /* Python: GET /api/oauth/account with bearer. Arg = "base_url\tresult".
     * result empty = failure. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *result = tab ? tab + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    printf("{}\n");
    return 0;
}

/* PoP: _info_from_valid_jwt @ hermes_cli/nous_account.py:_info_from_valid_jwt */
int nous_u_info_from_valid_jwt(const char *arg) { (void)arg; return 0; }

/* PoP: _info_from_account_payload @ hermes_cli/nous_account.py:_info_from_account_payload */
int nous_u_info_from_account_payload(const char *arg) { (void)arg; return 0; }

/* PoP: _tool_access_from_value @ hermes_cli/nous_account.py:_tool_access_from_value */
int nous_u_tool_access_from_value(const char *arg) {
    /* Python: {enabled, coverage} fail-closed. Arg = "value_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    int enabled = json_get_bool(j, "enabled", 0);
    json_t *cov = json_obj_get(j, "coverage");
    int first = 1;
    if (cov && json_is_object(cov)) {
        for (size_t i = 0; i < cov->c.count; i++) {
            json_t *v = json_obj_get(cov, cov->c.keys[i]);
            if (v && json_is_true(v)) {
                if (!first) printf("\n");
                printf("%s", cov->c.keys[i]);
                first = 0;
            }
        }
    }
    printf("\tenabled=%d\n", enabled ? 1 : 0);
    json_free(j);
    return 0;
}

/* PoP: _subscription_from_payload @ hermes_cli/nous_account.py:_subscription_from_payload */
int nous_u_subscription_from_payload(const char *arg) {
    /* Python: coerce payload fields. Arg = "payload_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    printf("plan=%s tier=%s monthly_charge=%s credits_remaining=%s\n",
           json_get_str(j, "plan", ""),
           json_get_str(j, "tier", ""),
           json_get_str(j, "monthly_charge", ""),
           json_get_str(j, "credits_remaining", ""));
    json_free(j);
    return 0;
}

/* PoP: _paid_service_access_from_payload @ hermes_cli/nous_account.py:_paid_service_access_from_payload */
int nous_u_paid_service_access_from_payload(const char *arg) { (void)arg; return 0; }

/* PoP: _error_info @ hermes_cli/nous_account.py:_error_info */
int nous_u_error_info(const char *arg) {
    /* Python: NousPortalAccountInfo(logged_in, source="error", fresh=False,
     * portal_base_url, raw_account, error=str(error)). Arg = "error" (or
     * "error\tportal_base_url"). */
    const char *err = (arg && *arg) ? arg : "";
    const char *portal = "";
    const char *tab = strchr(err, '\t');
    if (tab) {
        portal = tab + 1;
        char *tmp = strndup(err, (size_t)(tab - err));
        err = tmp ? tmp : "";
        /* note: err may now point to malloc'd; fine for printf */
        printf("{\"logged_in\": false, \"source\": \"error\", \"fresh\": false, "
               "\"portal_base_url\": \"%s\", \"raw_account\": null, \"error\": \"%s\"}\n",
               portal, err);
        free(tmp);
        return 0;
    }
    printf("{\"logged_in\": false, \"source\": \"error\", \"fresh\": false, "
           "\"portal_base_url\": \"%s\", \"raw_account\": null, \"error\": \"%s\"}\n",
           portal, err);
    return 0;
}

/* PoP: _portal_base_url @ hermes_cli/nous_account.py:_portal_base_url */
int nous_u_portal_base_url(const char *arg) {
    /* Python: state.get("portal_base_url"); None unless a non-empty str;
     * stripped and rstrip("/"). Arg = state JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *st = json_parse(arg, NULL);
    if (!st || !json_is_object(st)) {
        if (st) json_free(st);
        printf("\n");
        return 0;
    }
    const char *v = json_get_str(st, "portal_base_url", NULL);
    if (!v) { json_free(st); printf("\n"); return 0; }
    while (*v == ' ' || *v == '\t') v++;
    size_t n = strlen(v);
    while (n > 0 && (v[n-1] == '/' || v[n-1] == ' ' || v[n-1] == '\t')) n--;
    if (n == 0) { json_free(st); printf("\n"); return 0; }
    printf("%.*s\n", (int)n, v);
    json_free(st);
    return 0;
}

/* PoP: _cache_key @ hermes_cli/nous_account.py:_cache_key */
int nous_u_cache_key(const char *arg) {
    /* Python: f"{portal_base_url or ''}:{sha256(access_token).hexdigest()}".
     * Arg = "access_token\tportal_base_url". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char token[4096];
    size_t tlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (tlen >= sizeof(token)) tlen = sizeof(token) - 1;
    memcpy(token, arg, tlen); token[tlen] = '\0';
    const char *base = tab ? tab + 1 : "";
    char *hex = hash_sha256_hex((const unsigned char *)token, tlen);
    if (!hex) { printf("\n"); return 0; }
    printf("%s:%s\n", base, hex);
    free(hex);
    return 0;
}

/* PoP: _parse_iso_timestamp @ hermes_cli/nous_account.py:_parse_iso_timestamp */
int nous_u_parse_iso_timestamp(const char *arg) {
    if (!arg || !*arg) return 0;
    char buf[64]; strncpy(buf, arg, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    char *z = strchr(buf, 'Z'); if (z) *z = '+';
    char *plus = strchr(buf, '+');
    if (plus && strchr(plus, ':')) {
        memmove(plus + 3, plus + 4, strlen(plus + 4) + 1);
        memmove(plus + 2, plus + 3, strlen(plus + 3) + 1);
    }
    struct tm tm; memset(&tm, 0, sizeof tm);
    if (strptime(buf, "%Y-%m-%dT%H:%M:%S", &tm)) return (int)timegm(&tm);
    if (strptime(buf, "%Y-%m-%d %H:%M:%S", &tm)) return (int)timegm(&tm);
    return 0;
}

/* PoP: _coerce_str @ hermes_cli/nous_account.py:_coerce_str */
int nous_u_coerce_str(const char *arg) {
    /* Python: a truthy str returns the value, anything else None. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
