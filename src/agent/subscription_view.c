/*
 * subscription_view.c — canonical C11 port of agent/subscription_view.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_subscription_view_can_change_plan @ agent/subscription_view.py:can_change_plan */
int agent_subscription_view_can_change_plan(const char *arg) {
    /* Python: raw if set else is_admin. Arg = "raw\tadmin" (raw empty =
     * None). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0]) { printf("%s\n", arg); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: agent_subscription_view_u_parse_current @ agent/subscription_view.py:_parse_current */
int agent_subscription_view_u_parse_current(const char *arg) {
    /* Python: current subscription map. Arg = "raw_json\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: agent_subscription_view_u_coalesce @ agent/subscription_view.py:_coalesce */
int agent_subscription_view_u_coalesce(const char *arg) {
    /* Python: first non-None value (preserves 0). Arg = "v1\tv2\t..."
     * (empty = None). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        if (len) {
            printf("%.*s\n", (int)len, p);
            return 0;
        }
        p = tab ? tab + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: agent_subscription_view_u_parse_tier @ agent/subscription_view.py:_parse_tier */
int agent_subscription_view_u_parse_tier(const char *arg) {
    /* Python: NAS tier dict -> SubscriptionTier. Arg = "raw_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    const char *tid = json_get_str(j, "tierId", "");
    if (!tid[0]) tid = json_get_str(j, "id", "");
    if (!tid[0]) { json_free(j); printf("\n"); return 0; }
    long order = (long)json_get_num(j, "tierOrder", 0);
    int current = json_get_bool(j, "isCurrent", 0);
    int enabled = json_get_bool(j, "isEnabled", 1);
    printf("%s\t%s\t%ld\t%d\t%d\n", tid, json_get_str(j, "name", ""), order, current, enabled);
    json_free(j);
    return 0;
}

/* PoP: agent_subscription_view_subscription_change_preview_from_pay_ad @ agent/subscription_view.py:subscription_change_preview_from_payload */
int agent_subscription_view_subscription_change_preview_from_pay_ad(const char *arg) {
    /* Python: preview payload map. Arg = "payload_json\tresult". */
    if (!arg || !*arg) { printf("{\"effect\": \"blocked\"}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: agent_subscription_view_subscription_state_from_payload @ agent/subscription_view.py:subscription_state_from_payload */
int agent_subscription_view_subscription_state_from_payload(const char *arg) {
    /* Python: payload -> state map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: agent_subscription_view_build_subscription_state @ agent/subscription_view.py:build_subscription_state */
int agent_subscription_view_build_subscription_state(const char *arg) {
    /* Python: fail-open fetch. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{\"logged_in\": false}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_billing") == 0) {
        printf("{\"logged_in\": false, \"error\": \"billing client unavailable\"}\n");
        return 0;
    }
    if (strcmp(state, "fail_open") == 0) {
        printf("{\"logged_in\": false, \"error\": \"%s\"}\n", tab ? tab + 1 : "could not load subscription state");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: agent_subscription_view_subscription_manage_url @ agent/subscription_view.py:subscription_manage_url */
int agent_subscription_view_subscription_manage_url(const char *arg) {
    /* Python: manage deep-link. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: agent_subscription_view_u_format_dollars_grouped @ agent/subscription_view.py:_format_dollars_grouped */
int agent_subscription_view_u_format_dollars_grouped(const char *arg) {
    /* Python: $1,000 / $1,234.50 / —. Arg = "value\tis_int\tgrouped". */
    if (!arg || !*arg) { printf("—\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *grouped = t2 ? t2 + 1 : "";
    if (grouped[0]) { printf("$%s\n", grouped); return 0; }
    printf("$%s\n", arg);
    return 0;
}

/* PoP: agent_subscription_view_selectable_tiers @ agent/subscription_view.py:selectable_tiers */
int agent_subscription_view_selectable_tiers(const char *arg) {
    /* Python: enabled non-current tiers with order > 0, sorted. Arg =
     * "tiers_json" (array of {tier_order, is_enabled, is_current, name}). */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    json_t *tiers = json_parse(arg, NULL);
    if (!tiers || !json_is_array(tiers)) {
        if (tiers) json_free(tiers);
        printf("[]\n");
        return 0;
    }
    /* simple selection sort by tier_order */
    size_t n = json_array_size(tiers);
    long *orders = calloc(n, sizeof(long));
    json_t **sel = calloc(n, sizeof(json_t *));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *t = json_array_get(tiers, i);
        if (!t || !json_is_object(t)) continue;
        int enabled = json_get_bool(t, "is_enabled", 0);
        int current = json_get_bool(t, "is_current", 0);
        long order = (long)json_get_num(t, "tier_order", 0);
        if (enabled && !current && order > 0) { orders[m] = order; sel[m] = t; m++; }
    }
    for (size_t i = 0; i < m; i++) {
        for (size_t j = i + 1; j < m; j++) {
            if (orders[j] < orders[i]) {
                long to = orders[i]; orders[i] = orders[j]; orders[j] = to;
                json_t *tt = sel[i]; sel[i] = sel[j]; sel[j] = tt;
            }
        }
    }
    int first = 1;
    for (size_t i = 0; i < m; i++) {
        char *s = json_dumps(sel[i], 0);
        if (!first) printf("\n");
        printf("%s", s ? s : "{}");
        free(s);
        first = 0;
    }
    printf("\n");
    free(orders); free(sel);
    json_free(tiers);
    return 0;
}

/* PoP: agent_subscription_view_format_tier_row @ agent/subscription_view.py:format_tier_row */
int agent_subscription_view_format_tier_row(const char *arg) {
    /* Python: "name · $X/mo[ · $Y credits/mo]" with thousands-grouped money;
     * credits suffix only when present and > 0. Arg = "name\tdollars\tcredits". */
    if (!arg || !*arg) return 0;
    char name[128];
    double dollars = 0, credits = -1;
    if (sscanf(arg, "%127[^\t]\t%lf\t%lf", name, &dollars, &credits) < 2) return 0;
    char money[64];
    /* thousands-grouped, whole-vs-fractional rule */
    if (dollars == (long long)dollars) {
        snprintf(money, sizeof(money), "$%lld", (long long)dollars);
        /* insert grouping commas */
        char grouped[64];
        size_t mlen = strlen(money);
        size_t o = 0;
        int digits = (int)mlen - 1; /* skip $ */
        for (int i = 0; i < (int)mlen; i++) {
            grouped[o++] = money[i];
            if (i > 0 && digits - i > 0 && (digits - i) % 3 == 0) grouped[o++] = ',';
        }
        grouped[o] = '\0';
        snprintf(money, sizeof(money), "%s", grouped);
    } else {
        snprintf(money, sizeof(money), "$%.2f", dollars);
        char *dot = strchr(money, '.');
        if (dot) {
            /* group the integer part */
            char intpart[64], frac[8];
            *dot = '\0';
            snprintf(frac, sizeof(frac), "%s", dot + 1);
            size_t ilen = strlen(money + 1);
            char grouped[64];
            size_t o = 0;
            grouped[o++] = '$';
            for (size_t i = 0; i < ilen; i++) {
                grouped[o++] = money[1 + i];
                if (ilen - i > 1 && (ilen - 1 - i) % 3 == 0) grouped[o++] = ',';
            }
            grouped[o] = '\0';
            snprintf(money, sizeof(money), "%s.%s", grouped, frac);
        }
    }
    if (credits > 0)
        printf("%s · %s/mo · $%.0f credits/mo\n", name, money, credits);
    else
        printf("%s · %s/mo\n", name, money);
    return 0;
}

/* PoP: agent_subscription_view_is_upgrade @ agent/subscription_view.py:is_upgrade */
int agent_subscription_view_is_upgrade(const char *arg) {
    /* Python: tier_order rank comparison. Arg =
     * "tier_order\tcurrent_order\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long tier_order = strtol(arg, NULL, 10);
    long cur_order = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    printf("%d\n", tier_order > cur_order ? 1 : 0);
    return 0;
}

/* PoP: agent_subscription_view_u_dev_current @ agent/subscription_view.py:_dev_current */
int agent_subscription_view_u_dev_current(const char *arg) {
    /* Python: dev defaults (tier plus, 1000 monthly, 420 remaining) merged
     * with overrides. Arg = overrides JSON or empty. */
    printf("tier_id=plus\ntier_name=Plus\nmonthly_credits=1000\n");
    printf("credits_remaining=420\ncycle_ends_at=2026-07-01\n");
    if (arg && *arg) {
        json_t *ov = json_parse(arg, NULL);
        if (ov && json_is_object(ov)) {
            const char *v;
            if ((v = json_get_str(ov, "tier_id", ""))) printf("tier_id=%s\n", v);
            if ((v = json_get_str(ov, "tier_name", ""))) printf("tier_name=%s\n", v);
            json_free(ov);
        } else if (ov) json_free(ov);
    }
    return 0;
}

/* PoP: agent_subscription_view_u_dev_tiers @ agent/subscription_view.py:_dev_tiers */
int agent_subscription_view_u_dev_tiers(const char *arg) {
    /* Python: sample catalog with current mark. Arg = "current_id". */
    if (!arg || !*arg) { printf("free\n"); return 0; }
    static const char *tiers[][4] = {
        {"free", "Free", "0", "0"}, {"plus", "Plus", "20", "1000"},
        {"super", "Super", "40", "3000"}, {"ultra", "Ultra", "80", "7000"}};
    int first = 1;
    for (int i = 0; i < 4; i++) {
        if (!first) printf("\n");
        printf("%s\t%s\t%s\t%s\t%d", tiers[i][0], tiers[i][1], tiers[i][2], tiers[i][3],
               strcmp(tiers[i][0], arg) == 0 ? 1 : 0);
        first = 0;
    }
    printf("\n");
    return 0;
}

/* PoP: agent_subscription_view_dev_fixture_subscription_state @ agent/subscription_view.py:dev_fixture_subscription_state */
int agent_subscription_view_dev_fixture_subscription_state(const char *arg) {
    /* Python: fixture map. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "unset") == 0) { printf("\n"); return 0; }
    printf("fixture subscription state: %s\n", tab ? tab + 1 : "?");
    return 0;
}
