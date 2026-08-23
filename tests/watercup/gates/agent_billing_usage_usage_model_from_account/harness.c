/* gate: agent_billing_usage_usage_model_from_account — links the REAL
 * src/agent/billing_usage.c::usage_model_from_account via extra_src.
 * LIVE twin: agent/billing_usage.py::usage_model_from_account.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: no account object / not logged_in -> available=False
 *   B2: paid is literally false           -> depleted
 *   B3: no subscription && no topup       -> free
 *   B4: total spendable < $5              -> low
 *   B5: plan bar built   B6: topup bar built
 * VSTR(status|sub_rem@p|topup_rem@p|total@p|planbar rem@p/tot@p/spent@p|
 *      topbar rem@p/tot@p|plan|renews) — '@p' appends 1/0 presence flags;
 * doubles use %.17g on both sides.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "billing_usage.h"
#include "hermes_json.h"
#include "watercup.h"

/* billing_usage.c references this single external; provide a local
 * definition so the gate links without dragging in the HTTP stack.
 * It is NOT used by the traced path (renews_display is not hashed). */
#include "hermes_account_usage.h"
void account_usage_fmt_usd(double d, char *buf, size_t sz) { snprintf(buf, sz, "%.2f", d); }

usage_model_t *usage_model_from_account(const void *account_info_json);

static void fmt_d(char *buf, size_t sz, double v, int present) {
    if (present) snprintf(buf, sz, "%.17g", v);
    else snprintf(buf, sz, "-");
}

static void wc_case(const char *account_json) {
    WCUP_BEGIN("usage_model_from_account");

    json_t *ai = json_parse(account_json, NULL);
    int logged_in = 0;
    json_t *access = NULL, *sub = NULL;
    json_t *paid_node = NULL;
    if (ai && ai->type == JSON_OBJECT) {
        logged_in = json_get_bool(ai, "logged_in", false);
        access = json_obj_get(ai, "paid_service_access_info");
        sub = json_obj_get(ai, "subscription");
        paid_node = json_obj_get(ai, "paid_service_access");
    }
    int paid_is_false = (paid_node && paid_node->type == JSON_BOOL && paid_node->bool_val == false);
    WCUP_BRANCH(!(ai && ai->type == JSON_OBJECT) || !logged_in);   /* B1 */

    if (ai && ai->type == JSON_OBJECT && logged_in) {
        WCUP_BRANCH(paid_is_false);                                /* B2 */
        if (!paid_is_false) {
            double sub_r = 0, top_r = 0, tot_u = 0;
            int has_sub_r = access && billing_usage_finite(json_obj_get(access, "subscription_credits_remaining"), &sub_r);
            int has_top = access && billing_usage_finite(json_obj_get(access, "purchased_credits_remaining"), &top_r);
            int has_tot = access && billing_usage_finite(json_obj_get(access, "total_usable_credits"), &tot_u);
            double monthly = 0;
            int has_monthly = sub && billing_usage_finite(json_obj_get(sub, "monthly_credits"), &monthly);
            const char *plan = sub ? json_get_str(sub, "plan", NULL) : NULL;
            int has_subscription = (plan && plan[0]) || (has_monthly && monthly > 0);
            double ts = 0; int has_ts = 0;
            if (has_tot) { ts = tot_u; has_ts = 1; }
            else {
                int n = 0; double p[2];
                if (has_sub_r) p[n++] = sub_r;
                if (has_top) p[n++] = top_r;
                if (n) { for (int i = 0; i < n; i++) ts += p[i]; has_ts = 1; }
            }
            int free_case = (!has_subscription && !(has_top && top_r > 0));
            WCUP_BRANCH(free_case);                                /* B3 */
            if (!free_case) {
                int low = (has_ts && ts < 5.0);
                WCUP_BRANCH(low);                                  /* B4 */
            }
            int plan_bar = (has_monthly && monthly > 0 && has_sub_r);
            WCUP_BRANCH(plan_bar);                                 /* B5 */
            int top_bar = (has_top && top_r > 0);
            WCUP_BRANCH(top_bar);                                  /* B6 */
        }
    }

    usage_model_t *m = usage_model_from_account(ai);  /* port takes a parsed json_t* */
    char label[1024];
    if (!m) {
        snprintf(label, sizeof label, "NULL");
    } else if (!usage_model_available(m)) {
        snprintf(label, sizeof label, "unavailable");
    } else {
        const usage_bar_t *pb = usage_model_plan_bar(m);
        const usage_bar_t *tb = usage_model_topup_bar(m);
        char sr[64], tr[64], tt[64], pbr[64], pbt[64], pbs[64], tbr[64], tbt[64];
        fmt_d(sr, sizeof sr, usage_model_subscription_remaining(m), usage_model_has_subscription_remaining(m));
        fmt_d(tr, sizeof tr, usage_model_topup_remaining(m), usage_model_has_topup_remaining(m));
        fmt_d(tt, sizeof tt, usage_model_total_spendable(m), usage_model_has_total_spendable(m));
        if (pb) {
            fmt_d(pbr, sizeof pbr, usage_bar_remaining(pb), 1);
            fmt_d(pbt, sizeof pbt, usage_bar_total(pb), 1);
            fmt_d(pbs, sizeof pbs, usage_bar_spent(pb), 1);
        } else { strcpy(pbr, "-"); strcpy(pbt, "-"); strcpy(pbs, "-"); }
        if (tb) {
            fmt_d(tbr, sizeof tbr, usage_bar_remaining(tb), 1);
            fmt_d(tbt, sizeof tbt, usage_bar_total(tb), 1);
        } else { strcpy(tbr, "-"); strcpy(tbt, "-"); }
        snprintf(label, sizeof label, "%s|%s|%s|%s|%s/%s/%s|%s/%s|%s",
                 usage_model_status(m), sr, tr, tt, pbr, pbt, pbs, tbr, tbt,
                 usage_model_plan_name(m) ? usage_model_plan_name(m) : "-");
    }
    WCUP_VALSTR(label);
    fprintf(stderr, "LABEL[%s]\n", label);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("usage_model_from_account");
    }
    if (m) usage_model_free(m);
    if (ai) json_free(ai);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s usage_model_from_account <account-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_case(argv[i]);
    return 0;
}
